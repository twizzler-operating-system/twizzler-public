#include <init.h>
#include <object.h>
#include <page.h>
#include <pager.h>
#include <processor.h>
#include <slab.h>
#include <slots.h>
#include <syscall.h>
#include <thread.h>
#include <tmpmap.h>

#include <device.h>
#include <twz/_fault.h>
#include <twz/driver/queue.h>

#include <queue.h>

struct pager_request {
	struct queue_entry_pager pqe;
	struct rbnode node_id;
	struct rbnode node_obj;
	struct object *obj;
	struct objpage *objpage;
	struct thread *thread;
};

static struct rbroot root;
static struct spinlock pager_lock = SPINLOCK_INIT;

static struct object *pager_tmp_object = NULL;
static _Atomic size_t pager_tmp_object_pgnr = 0;
static void _sc_pr_ctor(void *p __unused, void *o)
{
	static _Atomic uint32_t pr_id = 0;
	struct pager_request *pr = o;
	pr->pqe.qe.info = pr_id++;

	size_t thispg = ++pager_tmp_object_pgnr;
	if(thispg >= OBJ_TOPDATA / mm_page_size(0)) {
		panic("TODO: too many outstanding requests");
	}
	/* TODO: we don't need to zero the page? */
	enum obj_get_page_result gpr =
	  obj_get_page(pager_tmp_object, thispg * mm_page_size(0), &pr->objpage, OBJ_GET_PAGE_ALLOC);
	if(gpr != GETPAGE_OK) {
		panic("failed to get page from pager tmp object");
	}
	// printk("pr_ctor: %ld\n", pr->objpage->idx);
}

static DECLARE_SLABCACHE(sc_pager_request, sizeof(struct pager_request), _sc_pr_ctor, NULL, NULL);

static int __pr_compar_key_obj(struct pager_request *a, size_t n)
{
	if(a->pqe.page > n)
		return 1;
	else if(a->pqe.page < n)
		return -1;
	return 0;
}

static int __pr_compar_obj(struct pager_request *a, struct pager_request *b)
{
	return __pr_compar_key_obj(a, b->pqe.page);
}

static int __pr_compar_key(struct pager_request *a, uint32_t n)
{
	if(a->pqe.qe.info > n)
		return 1;
	else if(a->pqe.qe.info < n)
		return -1;
	return 0;
}

static int __pr_compar(struct pager_request *a, struct pager_request *b)
{
	return __pr_compar_key(a, b->pqe.qe.info);
}

static void pager_init(void *a __unused)
{
	objid_t id;
	int r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_WRITE | MIP_DFL_READ, &id);
	assert(r == 0);
	pager_tmp_object = obj_lookup(id, 0);
	if(pager_tmp_object) {
		pager_tmp_object->flags |= OF_PINNED;
		obj_alloc_slot(pager_tmp_object);
	}
}
POST_INIT(pager_init, NULL);

static void reclaim_pr(struct pager_request *pr)
{
	pr->objpage->page = page_alloc(PAGE_TYPE_VOLATILE, 0, 0);
	arch_object_map_page(pager_tmp_object, pr->objpage);
	/* TODO: update this interface */
	arch_object_map_flush(pager_tmp_object, pr->objpage->idx * mm_page_size(0));
	iommu_invalidate_tlb();
	slabcache_free(&sc_pager_request, pr);
}

static void __complete_object(struct pager_request *pr, struct queue_entry_pager *pqe)
{
	if(pr->pqe.id != pqe->id) {
		printk("[kq] warning - completion ID mismatch\n");
		return;
	}
	// printk("complete object :: %d\n", pqe->result);
	if(pqe->result == PAGER_RESULT_DONE) {
		struct object *obj = obj_lookup(pqe->id, 0);
		if(!obj) {
			/* create it */
			obj = obj_create(pqe->id, 0);
		}
		/* need to make this atomic with create, and actually use the right flags */
		obj->flags |= OF_CPF_VALID | OF_PAGER;
		obj->cached_pflags = MIP_DFL_WRITE | MIP_DFL_READ | MIP_DFL_EXEC;
		obj_put(obj);
		pr->thread->pager_obj_req = 0;
	} else {
		pr->thread->pager_obj_req = 0;
		// struct fault_object_info info =
		//  twz_fault_build_object_info(pr->pqe.id, NULL /* TODO */, NULL, FAULT_OBJECT_EXIST);
		// thread_queue_fault(pr->thread, FAULT_OBJECT, &info, sizeof(info));
	}

	thread_wake(pr->thread);
}

static void __complete_page(struct pager_request *pr, struct queue_entry_pager *pqe)
{
	pr->thread->pager_obj_req = 0;
	if(pqe->id != pr->pqe.id || pqe->page != pr->pqe.page) {
		printk("[kq] warning - ID or page mismatch\n");
		goto cleanup;
	}
	// printk("complete page :: %d\n", pqe->result);
	switch(pqe->result) {
		case PAGER_RESULT_ZERO: {
			struct page *page = page_alloc(
			  pr->obj->flags & OF_PERSIST ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE, PAGE_ZERO, 0);
			obj_cache_page(pr->obj, pr->pqe.page * mm_page_size(0), page);
		} break;
		case PAGER_RESULT_DONE:
			obj_cache_page(pr->obj, pr->pqe.page * mm_page_size(0), pr->objpage->page);
			break;
		default:
		case PAGER_RESULT_ERROR: {
			struct fault_object_info info = twz_fault_build_object_info(
			  pr->pqe.id, NULL /* TODO */, NULL, FAULT_OBJECT_NOMAP /* TODO */);
			thread_queue_fault(pr->thread, FAULT_OBJECT, &info, sizeof(info));
		} break;
	}
cleanup:
	rb_delete(&pr->node_obj, &pr->obj->page_requests_root);
	thread_wake_object(pr->obj, pr->pqe.page * mm_page_size(0), ~0);
	obj_put(pr->obj);
	pr->obj = NULL;
}

static void _pager_fn(void)
{
	struct queue_hdr *hdr = kernel_queue_get_hdr(KQ_PAGER);
	struct object *qobj = kernel_queue_get_object(KQ_PAGER);
	spinlock_acquire_save(&pager_lock);

	struct queue_entry_pager pqe;
	if(kernel_queue_get_cmpls(qobj, hdr, (struct queue_entry *)&pqe) == 0) {
		struct rbnode *node =
		  rb_search(&root, pqe.qe.info, struct pager_request, node_id, __pr_compar_key);

		if(!node) {
			printk("[kq] warning - pager got completion for request it didn't know about\n");
			goto done;
		}

		struct pager_request *pr = rb_entry(node, struct pager_request, node_id);
		rb_delete(&pr->node_id, &root);

		if(pr->pqe.cmd == PAGER_CMD_OBJECT) {
			__complete_object(pr, &pqe);
		} else if(pr->pqe.cmd == PAGER_CMD_OBJECT_PAGE) {
			__complete_page(pr, &pqe);
		}
		reclaim_pr(pr);
	}

done:
	spinlock_release_restore(&pager_lock);

	obj_put(qobj);
}

void pager_idle_task(void)
{
	if(kernel_queue_get_hdr(KQ_PAGER)) {
		_pager_fn();
	}
}

int kernel_queue_pager_request_object(objid_t id)
{
	struct queue_hdr *hdr = kernel_queue_get_hdr(KQ_PAGER);
	struct object *qobj = kernel_queue_get_object(KQ_PAGER);
	if(!qobj || !hdr) {
		//	printk("[kq] warning - no pager registered\n");
		return -1;
	}
	//	printk("[kq] pager request object " IDFMT "\n", IDPR(id));
	bool new = current_thread->pager_obj_req == 0;

	if(!new) {
		// printk("[kq] this is a redo!\n");
		obj_put(qobj);
		return -1;
	}

	spinlock_acquire_save(&pager_lock);
	struct pager_request *pr = slabcache_alloc(&sc_pager_request);

	thread_sleep(current_thread, 0, -1);

	pr->pqe.id = id;
	pr->pqe.reqthread = current_thread->thrid;
	pr->pqe.cmd = PAGER_CMD_OBJECT;
	pr->pqe.result = 0;
	pr->thread = current_thread;

	if(kernel_queue_submit(qobj, hdr, (struct queue_entry *)&pr->pqe) == 0) {
		/* TODO: verify that this did not overwrite */
		rb_insert(&root, pr, struct pager_request, node_id, __pr_compar);
		// printk("[kq] enqueued! info = %d\n", pr->pqe.qe.info);
		current_thread->pager_obj_req = id;
	} else {
		printk("[kq] failed enqueue\n");
		thread_wake(current_thread);
	}
	spinlock_release_restore(&pager_lock);

	obj_put(qobj);
	return 0;
}

int kernel_queue_pager_request_page(struct object *obj, size_t pg)
{
	struct queue_hdr *hdr = kernel_queue_get_hdr(KQ_PAGER);
	struct object *qobj = kernel_queue_get_object(KQ_PAGER);
	if(!qobj || !hdr) {
		// printk("[kq] warning - no pager registered\n");
		return -1;
	}
	// printk("[kq] pager request page " IDFMT " :: %lx\n", IDPR(obj->id), pg);
	bool new = current_thread->pager_obj_req == 0;

	if(!new) {
		//	printk("[kq] not new\n");
		obj_put(qobj);
		return -1;
	}

	spinlock_acquire_save(&pager_lock);

	/* try to see if there's an outstanding request for this page on this object */
	if(rb_search(
	     &obj->page_requests_root, pg, struct pager_request, node_obj, __pr_compar_key_obj)) {
		thread_sleep_on_object(obj, pg * mm_page_size(0), 0, true);
		goto done;
	}

	struct pager_request *pr = slabcache_alloc(&sc_pager_request);
	// thread_sleep(current_thread, 0, -1);
	thread_sleep_on_object(obj, pg * mm_page_size(0), 0, true);

	pr->pqe.id = obj->id;
	pr->pqe.page = pg;
	pr->pqe.reqthread = current_thread->thrid;
	pr->pqe.cmd = PAGER_CMD_OBJECT_PAGE;
	pr->pqe.result = 0;
	//	pr->pp =
	//	  page_alloc(obj->flags & OF_PERSIST ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE,
	// PAGE_ZERO,
	// 0);

#if 0
	static size_t tmppgnr = 0;
	size_t i = 0;

	size_t maxtmppgnr = OBJ_TOPDATA / mm_page_size(0);
	struct objpage *tmppg = NULL;
	for(i = tmppgnr + 1; (i % maxtmppgnr != tmppgnr) || !(i % maxtmppgnr); i++) {
		printk("::: ogp %ld\n", i % maxtmppgnr);
		obj_get_page(
		  pager_tmp_object, (i % maxtmppgnr) * mm_page_size(0), &tmppg, OBJ_GET_PAGE_ALLOC);
		break;
	}
	if(i == tmppgnr) {
		/* TODO: too many outstanding requests */
		panic(" ");
	}
	tmppgnr = i % maxtmppgnr;
	pr->objpage = tmppg;
	printk("::: %ld\n", tmppgnr);
#endif
	pr->thread = current_thread;
	pr->pqe.linaddr =
	  pager_tmp_object->slot->num * OBJ_MAXSIZE + pr->objpage->idx * mm_page_size(0);
	pr->pqe.tmpobjid = pager_tmp_object->id;

	if(kernel_queue_submit(qobj, hdr, (struct queue_entry *)&pr->pqe) == 0) {
		/* TODO: verify that this did not overwrite */
		rb_insert(&root, pr, struct pager_request, node_id, __pr_compar);

		krc_get(&obj->refs);
		pr->obj = obj;
		rb_insert(&obj->page_requests_root, pr, struct pager_request, node_obj, __pr_compar_obj);

		current_thread->pager_obj_req = obj->id;
	} else {
		printk("[kq] failed enqueue\n");
		thread_wake(current_thread);
	}

done:
	spinlock_release_restore(&pager_lock);

	obj_put(qobj);
	return 0;
}
