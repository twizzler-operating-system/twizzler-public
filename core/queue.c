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

struct object *kernel_queue_get_object(enum kernel_queues kq)
{
	spinlock_acquire_save(&queue_lock);
	struct object *obj = queue_objects[kq];
	if(obj) {
		krc_get(&obj->refs);
	}
	spinlock_release_restore(&queue_lock);
	return obj;
}

struct queue_hdr *kernel_queue_get_hdr(enum kernel_queues kq)
{
	spinlock_acquire_save(&queue_lock);
	struct queue_hdr *hdr = queue_hdrs[kq];
	spinlock_release_restore(&queue_lock);
	return hdr;
}
