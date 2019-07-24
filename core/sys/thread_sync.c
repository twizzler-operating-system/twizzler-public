#include <lib/inthash.h>
#include <memory.h>
#include <object.h>
#include <processor.h>
#include <slab.h>
#include <syscall.h>

struct syncpoint {
	struct object *obj;
	size_t off;
	int flags;
	struct krc refs;

	struct spinlock lock;
	struct list waiters;
	struct ihelem elem;
};

DECLARE_SLABCACHE(sc_syncpoint, sizeof(struct syncpoint), NULL, NULL, NULL);

/* TODO: do we need to make sure that this is enough syncronization. Do we need
 * to lock the relevant pagecache pages? */

/* TODO: determine if timeout occurred */

static struct syncpoint *sp_lookup(struct object *obj, size_t off, bool create)
{
	spinlock_acquire_save(&obj->tslock);
	struct syncpoint *sp = ihtable_find(obj->tstable, off, struct syncpoint, elem, off);
	if(!sp || !krc_get_unless_zero(&sp->refs)) {
		if(!create) {
			spinlock_release_restore(&obj->tslock);
			return NULL;
		}
		sp = slabcache_alloc(&sc_syncpoint);
		krc_get(&obj->refs);
		sp->obj = obj;
		sp->off = off;
		sp->flags = 0;
		krc_init(&sp->refs);
		list_init(&sp->waiters);
		ihtable_insert(obj->tstable, &sp->elem, sp->off);
	}
	spinlock_release_restore(&obj->tslock);
	return sp;
}

static void _sp_release(void *_sp)
{
	struct syncpoint *sp = _sp;
	spinlock_acquire_save(&sp->obj->tslock);
	ihtable_remove(sp->obj->tstable, &sp->elem, sp->off);
	spinlock_release_restore(&sp->obj->tslock);
	slabcache_free(sp);
}

static int sp_sleep_prep(struct syncpoint *sp, long *addr, long val, struct timespec *spec)
{
	int64_t ns = spec ? (spec->tv_nsec + spec->tv_sec * 1000000000ul) : -1ul;
	spinlock_acquire_save(&sp->lock);
	spinlock_acquire_save(&current_thread->lock);
	thread_sleep(current_thread, 0, ns);
	list_insert(&sp->waiters, &current_thread->rq_entry);

	int r = atomic_load(addr) == val;
	spinlock_release_restore(&current_thread->lock);
	spinlock_release_restore(&sp->lock);
	return r;
}

static void sp_sleep_finish(struct syncpoint *sp, int stay_asleep)
{
	if(stay_asleep)
		return;
	spinlock_acquire_save(&sp->lock);
	spinlock_acquire_save(&current_thread->lock);
	if(current_thread->state == THREADSTATE_BLOCKED) {
		list_remove(&current_thread->rq_entry);
		thread_wake(current_thread);
	}
	spinlock_release_restore(&current_thread->lock);
	spinlock_release_restore(&sp->lock);
	krc_put_call(sp, refs, _sp_release);
}

static int sp_sleep(struct syncpoint *sp, long *addr, long val, struct timespec *spec)
{
	sp_sleep_finish(sp, sp_sleep_prep(sp, addr, val, spec));
	return 0;
}

static int sp_wake(struct syncpoint *sp, long arg)
{
	if(!sp) {
		return 0;
	}
	spinlock_acquire_save(&sp->lock);
	struct list *next;
	int count = 0;
	for(struct list *e = list_iter_start(&sp->waiters); e != list_iter_end(&sp->waiters);
	    e = next) {
		next = list_iter_next(e);
		struct thread *t = list_entry(e, struct thread, rq_entry);
		if(arg == 0)
			break;
		else if(arg > 0)
			arg--;

		list_remove(&t->rq_entry);
		thread_wake(t);
		krc_put_call(sp, refs, _sp_release);
		count++;
	}
	spinlock_release_restore(&sp->lock);
	krc_put_call(sp, refs, _sp_release);
	return count;
}

static long thread_sync_single_norestore(int operation, long *addr, long arg, struct timespec *spec)
{
	objid_t id;
	uint64_t off;
	if(!vm_vaddr_lookup(addr, &id, &off)) {
		return -1; /* TODO (major): err codes in all syscalls */
	}
	struct object *obj = obj_lookup(id);
	if(!obj) {
		return -1;
	}
	struct syncpoint *sp = sp_lookup(obj, off, operation == THREAD_SYNC_SLEEP);
	obj_put(obj);
	switch(operation) {
		case THREAD_SYNC_SLEEP:
			return sp_sleep_prep(sp, addr, arg, spec);
		case THREAD_SYNC_WAKE:
			return sp_wake(sp, arg);
		default:
			break;
	}
	return -1;
}

static long thread_sync_sleep_wakeup(long *addr, int wake)
{
	objid_t id;
	uint64_t off;
	if(!vm_vaddr_lookup(addr, &id, &off)) {
		return -1; /* TODO (major): err codes in all syscalls */
	}
	struct object *obj = obj_lookup(id);
	if(!obj) {
		return -1;
	}
	struct syncpoint *sp = sp_lookup(obj, off, true);
	obj_put(obj);
	sp_sleep_finish(sp, 0);
	return 0;
}

long thread_sync_single(int operation, long *addr, long arg, struct timespec *spec)
{
	objid_t id;
	uint64_t off;
	if(!vm_vaddr_lookup(addr, &id, &off)) {
		return -1; /* TODO (major): err codes in all syscalls */
	}
	struct object *obj = obj_lookup(id);
	if(!obj) {
		return -1;
	}
	struct syncpoint *sp = sp_lookup(obj, off, operation == THREAD_SYNC_SLEEP);
	obj_put(obj);
	switch(operation) {
		case THREAD_SYNC_SLEEP:
			return sp_sleep(sp, addr, arg, spec);
		case THREAD_SYNC_WAKE:
			return sp_wake(sp, arg);
		default:
			break;
	}
	return -1;
}

long syscall_thread_sync(size_t count,
  int *operation,
  long **addr,
  long *arg,
  long *res,
  struct timespec **spec)
{
	bool ok = false;
	bool wake = false;
	for(size_t i = 0; i < count; i++) {
		int r = thread_sync_single_norestore(operation[i], addr[i], arg[i], spec ? spec[i] : NULL);
		ok = ok || r >= 0;
		if(res)
			res[i] = 1; // TODO
		if(operation[i] == THREAD_SYNC_SLEEP && r == 0)
			wake = true;
	}
	if(wake) {
		for(size_t i = 0; i < count; i++) {
			if(operation[i] == THREAD_SYNC_SLEEP)
				thread_sync_sleep_wakeup(addr[i], 1);
		}
	}
	return ok ? 0 : -1;
}
