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
static _Atomic size_t nr_outstanding = 0;
static struct task pager_task;

static void _sc_pr_ctor(void *p __unused, void *o)
{
	static _Atomic uint32_t pr_id = 0;
	struct pager_request *pr = o;
	pr->pqe.qe.info = pr_id++;
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

static struct object *pager_tmp_object = NULL;

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

static void _pager_fn(void *a)
{
	(void)a;

	struct queue_hdr *hdr = kernel_queue_get_hdr(KQ_PAGER);
	struct object *qobj = kernel_queue_get_object(KQ_PAGER);
	// printk("call to pager_fn\n");
	spinlock_acquire_save(&pager_lock);

	struct queue_entry_pager pqe;
	if(kernel_queue_get_cmpls(qobj, hdr, (struct queue_entry *)&pqe) == 0) {
		printk("[kq] got completion %d for " IDFMT "\n", pqe.qe.info, IDPR(pqe.id));
		assert(nr_outstanding > 0);
		nr_outstanding--;

		struct rbnode *node =
		  rb_search(&root, pqe.qe.info, struct pager_request, node_id, __pr_compar_key);

		if(!node) {
			printk("[kq] warning - pager got completion for request it didn't know about\n");
			goto done;
		}

		struct pager_request *pr = rb_entry(node, struct pager_request, node_id);
		rb_delete(&pr->node_id, &root);
		/* TODO: check ID match, etc */

		if(pr->pqe.cmd == PAGER_CMD_OBJECT) {
			printk("[kq] completion is for object\n");
			struct object *tobj = obj_lookup(pqe.reqthread, 0);
			if(pqe.result == PAGER_RESULT_DONE) {
				struct object *obj = obj_lookup(pqe.id, 0);
				if(!obj) {
					/* create it */
					obj = obj_create(pqe.id, 0);
				}
				printk("[kq] ok!\n");
				/* need to make this atomic with create */
				obj->flags |= OF_KERNEL | OF_CPF_VALID | OF_PAGER;
				obj->cached_pflags = MIP_DFL_WRITE | MIP_DFL_READ;
				obj_put(obj);
				pr->thread->pager_obj_req = 0;
				// tobj->thr.thread->pager_obj_req = 0;
			} else {
				/* TODO: error */
			}

			if(tobj && tobj->kso_type == KSO_THREAD) {
				thread_wake(tobj->thr.thread);
			} else {
				printk("[kq] pager completed request for dead thread or non-thread\n");
			}

		} else if(pr->pqe.cmd == PAGER_CMD_OBJECT_PAGE) {
			printk("[kq] completion is for page\n");
			rb_delete(&pr->node_obj, &pr->obj->page_requests_root);

			thread_wake_object(pr->obj, pr->pqe.page * mm_page_size(0), ~0);
			pr->thread->pager_obj_req = 0;
			switch(pqe.result) {
				case PAGER_RESULT_ZERO:
					printk("[kq] result is to zero a page\n");
					/* TODO: release resources */
					obj_get_page(pr->obj, pr->pqe.page * mm_page_size(0), true);
					break;
				case PAGER_RESULT_COPY:
					printk("[kq] result is to copy a page from " IDFMT " :: %lx\n",
					  IDPR(pqe.id),
					  pqe.page);

					/* TODO: release resources */
					{
						struct object *obj = obj_lookup(pqe.id, 0);
						if(obj) {
							printk("[kq] copying page!\n");
							void *base = obj_get_kaddr(obj);
							struct page *pp =
							  page_alloc((pr->obj->flags & OF_PERSIST) ? PAGE_TYPE_PERSIST
							                                           : PAGE_TYPE_VOLATILE,
							    0,
							    0);
							void *mp = tmpmap_map_page(pp);
							/* TODO: bounds check */
							memcpy(mp, base + pqe.page * mm_page_size(0), mm_page_size(0));
							obj_cache_page(pr->obj, pr->pqe.page * mm_page_size(0), pp);
							tmpmap_unmap_page(mp);
							obj_release_kaddr(obj);
						} else {
							/* TODO: error */
						}
					}

					break;
				case PAGER_RESULT_DONE:
					// tmpmap_unmap_page(pr->tmpaddr);
					// obj_cache_page(pr->obj, pr->pqe.page * mm_page_size(0), pr->pp);
					printk("::: completed %ld %lx\n", pr->pqe.page, pr->objpage->page->addr);
					obj_cache_page(pr->obj, pr->pqe.page * mm_page_size(0), pr->objpage->page);
					break;
				default:
				case PAGER_RESULT_ERROR:
					thread_exit();
					/* TODO: error */
					break;
			}
			obj_put(pr->obj);
			pr->obj = NULL;
		}
	}

done:
	if(nr_outstanding && !a) {
		workqueue_insert(&current_processor->wq, &pager_task, _pager_fn, NULL);
	}
	spinlock_release_restore(&pager_lock);

	obj_put(qobj);
}

void pager_idle_task(void)
{
	if(kernel_queue_get_hdr(KQ_PAGER)) {
		_pager_fn((void *)1);
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
	printk("[kq] pager request object " IDFMT "\n", IDPR(id));
	bool new = current_thread->pager_obj_req == 0;

	if(!new) {
		printk("[kq] this is a redo!\n");
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
		printk("[kq] enqueued! info = %d\n", pr->pqe.qe.info);
		current_thread->pager_obj_req = id;
		if(nr_outstanding++ == 0) {
			workqueue_insert(&current_processor->wq, &pager_task, _pager_fn, NULL);
		}
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
		printk("[kq] warning - no pager registered\n");
		return -1;
	}
	printk("[kq] pager request page " IDFMT " :: %lx\n", IDPR(obj->id), pg);
	bool new = current_thread->pager_obj_req == 0;

	if(!new) {
		printk("[kq] not new\n");
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
	//	  page_alloc(obj->flags & OF_PERSIST ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE, PAGE_ZERO,
	// 0);

	static size_t tmppgnr = 0;
	size_t i = 0;
	size_t maxtmppgnr = OBJ_TOPDATA / mm_page_size(0);
	struct objpage *tmppg = NULL;
	for(i = tmppgnr + 1; (i % maxtmppgnr != tmppgnr) || !(i % maxtmppgnr); i++) {
		tmppg = obj_get_page(pager_tmp_object, (i % maxtmppgnr) * mm_page_size(0), true);
		break;
	}
	if(i == tmppgnr) {
		/* TODO: too many outstanding requests */
		panic(" ");
	}
	tmppgnr = i % maxtmppgnr;
	// tmppg = obj_get_page(pager_tmp_object, i, true);
	printk("[kq] tmpobj " IDFMT " page %p %ld: %lx\n",
	  IDPR(pager_tmp_object->id),
	  tmppg,
	  i,
	  tmppg->page->addr);
	pr->objpage = tmppg;
	pr->thread = current_thread;
	pr->pqe.linaddr =
	  pager_tmp_object->slot->num * OBJ_MAXSIZE + (i % maxtmppgnr) * mm_page_size(0);
	pr->pqe.tmpobjid = pager_tmp_object->id;
	printk("[kq]: linaddr: %lx\n", pr->pqe.linaddr);

	// pr->tmpaddr = tmpmap_map_page(pr->pp);

	// pr->pqe.linaddr = mm_vtoo(pr->tmpaddr);
	// pr->pqe.linaddr = obj->slot->num * OBJ_MAXSIZE + pg * mm_page_size(0);

	if(kernel_queue_submit(qobj, hdr, (struct queue_entry *)&pr->pqe) == 0) {
		/* TODO: verify that this did not overwrite */
		rb_insert(&root, pr, struct pager_request, node_id, __pr_compar);
		printk("[kq] enqueued! info %d\n", pr->pqe.qe.info);

		krc_get(&obj->refs);
		pr->obj = obj;
		rb_insert(&obj->page_requests_root, pr, struct pager_request, node_obj, __pr_compar_obj);

		current_thread->pager_obj_req = obj->id;
		if(nr_outstanding++ == 0) {
			workqueue_insert(&current_processor->wq, &pager_task, _pager_fn, NULL);
		}
	} else {
		printk("[kq] failed enqueue\n");
		thread_wake(current_thread);
	}

done:
	spinlock_release_restore(&pager_lock);

	obj_put(qobj);
	return 0;
}
