#include <syscall.h>
#include <processor.h>
#include <slab.h>
#include <lib/inthash.h>
#include <object.h>
#include <memory.h>

struct syncpoint {
	struct object *obj;
	size_t off;
	int flags;

	struct spinlock lock;
	struct list waiters;
	struct ihelem elem;

};

DECLARE_SLABCACHE(sc_syncpoint, sizeof(struct syncpoint), NULL, NULL, NULL);

/* TODO: do we need to make sure that this is enough syncronization. Do we need
 * to lock the relevant pagecache pages? */

static struct syncpoint *sp_lookup(struct object *obj, size_t off, bool create)
{
	spinlock_acquire_save(&obj->tslock);
	struct syncpoint *sp = ihtable_find(obj->tstable, off, struct syncpoint, elem, off);
	if(!sp && create) {
		sp = slabcache_alloc(&sc_syncpoint);
		sp->obj = obj;
		sp->off = off;
		sp->flags = 0;
		list_init(&sp->waiters);
		ihtable_insert(obj->tstable, &sp->elem, sp->off);
	}
	spinlock_release_restore(&obj->tslock);
	return sp;
}

static int sp_sleep(struct syncpoint *sp, int *addr, int val, struct timespec *spec)
{
	int64_t ns = spec ? (spec->tv_nsec + spec->tv_sec * 1000000000ul) : -1ul;
	thread_sleep(current_thread, 0, ns);
	spinlock_acquire_save(&sp->lock);
	list_insert(&sp->waiters, &current_thread->rq_entry);
	if(atomic_load(addr) != val) {
		list_remove(&current_thread->rq_entry);
		spinlock_release_restore(&sp->lock);
		thread_wake(current_thread);
	} else {
		spinlock_release_restore(&sp->lock);
	}
	return 0;
}

static int sp_wake(struct syncpoint *sp, int arg)
{
	if(!sp) {
		return 0;
	}
	spinlock_acquire_save(&sp->lock);
	struct list *next;
	int count = 0;
	for(struct list *e = list_iter_start(&sp->waiters);
			e != list_iter_end(&sp->waiters);
			e = next) {
		next = list_iter_next(e);
		struct thread *t = list_entry(e, struct thread, rq_entry);
		if(arg == 0) break;
		else if(arg > 0) arg--;

		thread_wake(t);
		count++;
	}
	spinlock_release_restore(&sp->lock);
	return count;
}

long syscall_thread_sync(int operation, int *addr, int arg, struct timespec *spec)
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
	switch(operation) {
		case THREAD_SYNC_SLEEP:
			return sp_sleep(sp, addr, arg, spec);
		case THREAD_SYNC_WAKE:
			return sp_wake(sp, arg);
		default: break;
	}
	return -1;
}

