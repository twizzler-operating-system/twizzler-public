#include <debug.h>
#include <object.h>
#include <processor.h>
#include <twz/_queue.h>
#include <twz/_sys.h>

#include <queue.h>
#include <twz/driver/queue.h>

int kernel_queue_wait_on(struct object *obj __unused, void *p __unused, uint64_t v __unused)
{
	panic("kernel tried to wait on queue");
}

int kernel_queue_wake_up(struct object *obj, void *p, uint64_t v)
{
	return thread_wake_object(obj, (long)p % OBJ_MAXSIZE, v);
	//	return thread_sync_single(THREAD_SYNC_WAKE, p, v, NULL);
}

int kernel_queue_submit(struct object *obj, struct queue_hdr *hdr, struct queue_entry *qe)
{
	return queue_sub_enqueue(obj, hdr, SUBQUEUE_SUBM, qe, true /* we always are non-blocking */);
}

int kernel_queue_get_cmpls(struct object *obj, struct queue_hdr *hdr, struct queue_entry *qe)
{
	return queue_sub_dequeue(obj, hdr, SUBQUEUE_CMPL, qe, true /* we always are non-blocking */);
}

static struct spinlock queue_lock = SPINLOCK_INIT;
static struct object *queue_objects[NUM_KERNEL_QUEUES] = {};
static struct queue_hdr *queue_hdrs[NUM_KERNEL_QUEUES] = {};

static const char *kernel_queue_names[] = {
	[KQ_PAGER] = "pager",
};

int kernel_queue_assign(enum kernel_queues kq, struct object *obj)
{
	spinlock_acquire_save(&queue_lock);
	if(queue_objects[kq]) {
		spinlock_release_restore(&queue_lock);
		return -EEXIST;
	}

	printk("[kq] registered " IDFMT " as queue for %s\n", IDPR(obj->id), kernel_queue_names[kq]);
	krc_get(&obj->refs);
	queue_objects[kq] = obj;

	queue_hdrs[kq] = obj_get_kbase(obj);

	spinlock_release_restore(&queue_lock);

	// struct queue_entry qe;
	// qe.info = 0x1234;
	// kernel_queue_submit(queue_hdrs[kq], &qe);
	return 0;
}

static struct spinlock pager_lock = SPINLOCK_INIT;
static _Atomic size_t nr_outstanding = 0;
static struct task pager_task;

static void _pager_fn(void *a)
{
	(void)a;

	printk("call to pager_fn\n");
	spinlock_acquire_save(&pager_lock);

	struct queue_entry_pager pqe;
	if(kernel_queue_get_cmpls(queue_objects[KQ_PAGER], queue_hdrs[KQ_PAGER], (struct queue_entry *)&pqe) == 0) {
		printk("[kq] got completion for " IDFMT "\n", IDPR(pqe.id));
		assert(nr_outstanding > 0);
		nr_outstanding--;

		if(pqe.id != 0) {
			struct object *obj = obj_lookup(pqe.id, 0);
			if(!obj) {
				/* create it */
				obj = obj_create(pqe.id, 0);
			}
			/* need to make this atomic with create */
			obj->flags |= OF_KERNEL | OF_CPF_VALID | OF_PAGER;
			obj->cached_pflags = MIP_DFL_WRITE | MIP_DFL_READ;
			obj_put(obj);
		}

		struct object *tobj = obj_lookup(pqe.reqthread, 0);
		if(tobj && tobj->kso_type == KSO_THREAD) {
			tobj->thr.thread->pager_obj_req = 0;
			thread_wake(tobj->thr.thread);
		} else {
			printk("[kq] pager completed request for dead thread or non-thread\n");
		}
	}

	if(nr_outstanding) {
		workqueue_insert(&current_processor->wq, &pager_task, _pager_fn, NULL);
	}
	spinlock_release_restore(&pager_lock);
}

int kernel_queue_pager_request_object(objid_t id)
{
	if(!queue_objects[KQ_PAGER] || !queue_hdrs[KQ_PAGER]) {
		printk("[kq] warning - no pager registered\n");
		return -1;
	}
	printk("[kq] pager request object " IDFMT "\n", IDPR(id));
	bool new = current_thread->pager_obj_req == 0;

	if(!new) {
		return -1;
	}

	thread_sleep(current_thread, 0, -1);

	struct queue_entry_pager pqe = {
		.qe.info = PAGER_CMD_OBJECT,
		.id = id,
		.reqthread = current_thread->thrid,
		.tid = current_thread->id,
	};

	spinlock_acquire_save(&pager_lock);
	if(kernel_queue_submit(queue_objects[KQ_PAGER], queue_hdrs[KQ_PAGER], (struct queue_entry *)&pqe) == 0) {
		printk("[kq] enqueued!\n");
		current_thread->pager_obj_req = id;
		if(nr_outstanding++ == 0) {
			workqueue_insert(&current_processor->wq, &pager_task, _pager_fn, NULL);
		}
	} else {
		printk("[kq] failed enqueue\n");
		thread_wake(current_thread);
	}
	spinlock_release_restore(&pager_lock);

	return 0;
}

int kernel_queue_pager_request_page(struct object *obj, size_t pg)
{
	if(!queue_objects[KQ_PAGER] || !queue_hdrs[KQ_PAGER]) {
		printk("[kq] warning - no pager registered\n");
		return -1;
	}
	printk("[kq] pager request page " IDFMT " :: %lx\n", IDPR(obj->id), pg);
	bool new = current_thread->pager_obj_req == 0;

	if(!new) {
		return -1;
	}

	thread_sleep(current_thread, 0, -1);

	struct queue_entry_pager pqe = {
		.qe.info = PAGER_CMD_OBJECT_PAGE,
		.id = obj->id,
		.reqthread = current_thread->thrid,
		.tid = current_thread->id,
		.page = pg,
	};

	spinlock_acquire_save(&pager_lock);
	if(kernel_queue_submit(queue_objects[KQ_PAGER], queue_hdrs[KQ_PAGER], (struct queue_entry *)&pqe) == 0) {
		printk("[kq] enqueued!\n");
		current_thread->pager_obj_req = obj->id;
		if(nr_outstanding++ == 0) {
			workqueue_insert(&current_processor->wq, &pager_task, _pager_fn, NULL);
		}
	} else {
		printk("[kq] failed enqueue\n");
		thread_wake(current_thread);
	}
	spinlock_release_restore(&pager_lock);

	return 0;
}
