#include <kalloc.h>
#include <lib/inthash.h>
#include <memory.h>
#include <object.h>
#include <processor.h>
#include <slab.h>
#include <syscall.h>

#define MAX_SLEEPS 1024

struct syncpoint {
	struct object *obj;
	size_t off;
	int flags;
	struct krc refs;

	struct spinlock lock;
	struct list waiters;
	struct rbnode node;
};

DECLARE_SLABCACHE(sc_syncpoint, sizeof(struct syncpoint), NULL, NULL, NULL);

static int __sp_compar_key(struct syncpoint *a, size_t n)
{
	if(a->off > n)
		return 1;
	else if(a->off < n)
		return -1;
	return 0;
}

static int __sp_compar(struct syncpoint *a, struct syncpoint *b)
{
	return __sp_compar_key(a, b->off);
}

/* TODO: do we need to make sure that this is enough syncronization. Do we need
 * to lock the relevant pagecache pages? */

/* TODO: determine if timeout occurred */

static struct syncpoint *sp_lookup(struct object *obj, size_t off, bool create)
{
	spinlock_acquire_save(&obj->tslock);
	struct rbnode *node =
	  rb_search(&obj->tstable_root, off, struct syncpoint, node, __sp_compar_key);
	struct syncpoint *sp = node ? rb_entry(node, struct syncpoint, node) : NULL;
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
		rb_insert(&obj->tstable_root, sp, struct syncpoint, node, __sp_compar);
	}
	spinlock_release_restore(&obj->tslock);
	return sp;
}

static void _sp_release(void *_sp)
{
	struct syncpoint *sp = _sp;
	spinlock_acquire_save(&sp->obj->tslock);
	rb_delete(&sp->node, &sp->obj->tstable_root);
	spinlock_release_restore(&sp->obj->tslock);
	struct object *obj = sp->obj;
	sp->obj = NULL;
	obj_put(obj);
	slabcache_free(&sc_syncpoint, sp);
}

static int sp_sleep_prep(struct syncpoint *sp, long *addr, long val, struct timespec *spec, int idx)
{
	int64_t ns = spec ? (spec->tv_nsec + spec->tv_sec * 1000000000ul) : -1ul;
	spinlock_acquire_save(&sp->lock);
	spinlock_acquire_save(&current_thread->lock);
	thread_sleep(current_thread, 0, ns);
	current_thread->sleep_entries[idx].thr = current_thread;
	current_thread->sleep_entries[idx].active = true;
	current_thread->sleep_entries[idx].sp = sp;
	list_insert(&sp->waiters, &current_thread->sleep_entries[idx].entry);

	/* TODO: verify that addr is a valid address that we can access */
	int r = atomic_load(addr) == val;
	spinlock_release_restore(&current_thread->lock);
	spinlock_release_restore(&sp->lock);
	return r;
}

static void sp_sleep_finish(struct syncpoint *sp, int stay_asleep, int idx)
{
	if(stay_asleep)
		return;
	spinlock_acquire_save(&sp->lock);
	spinlock_acquire_save(&current_thread->lock);
	if(current_thread->sleep_entries[idx].active) {
		list_remove(&current_thread->sleep_entries[idx].entry);
		current_thread->sleep_entries[idx].active = false;
	}

	if(current_thread->state == THREADSTATE_BLOCKED) {
		thread_wake(current_thread);
	}
	spinlock_release_restore(&current_thread->lock);
	spinlock_release_restore(&sp->lock);
	krc_put_call(sp, refs, _sp_release);
}

static int sp_sleep(struct syncpoint *sp, long *addr, long val, struct timespec *spec, int idx)
{
	sp_sleep_finish(sp, sp_sleep_prep(sp, addr, val, spec, idx), idx);
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
		struct sleep_entry *se = list_entry(e, struct sleep_entry, entry);
		if(arg == 0)
			break;
		else if(arg > 0)
			arg--;

		struct thread *thr = se->thr;
		spinlock_acquire_save(&thr->lock);

		list_remove(&se->entry);
		se->active = false;
		for(size_t i = 0; i < thr->sleep_count; i++) {
			if(thr->sleep_entries[i].active && se != &thr->sleep_entries[i]) {
				struct syncpoint *op = thr->sleep_entries[i].sp;
				spinlock_acquire_save(&op->lock);
				list_remove(&thr->sleep_entries[i].entry);
				thr->sleep_entries[i].active = false;
				spinlock_release_restore(&op->lock);
				krc_put_call(op, refs, _sp_release);
			}
		}

		thread_wake(se->thr);
		spinlock_release_restore(&thr->lock);
		krc_put_call(sp, refs, _sp_release);
		count++;
	}
	spinlock_release_restore(&sp->lock);
	krc_put_call(sp, refs, _sp_release);
	return count;
}

static long thread_sync_single_norestore(int operation,
  long *addr,
  long arg,
  struct timespec *spec,
  int idx)
{
	objid_t id;
	uint64_t off;
	if(!vm_vaddr_lookup(addr, &id, &off)) {
		return -1; /* TODO (major): err codes in all syscalls */
	}
	struct object *obj = obj_lookup(id, 0);
	if(!obj) {
		return -1;
	}
	struct syncpoint *sp = sp_lookup(obj, off, operation == THREAD_SYNC_SLEEP);
	long ret = -1;
	switch(operation) {
		case THREAD_SYNC_SLEEP:
			ret = sp_sleep_prep(sp, addr, arg, spec, idx);
			break;
		case THREAD_SYNC_WAKE:
			ret = sp_wake(sp, arg);
			break;
		default:
			break;
	}
	obj_put(obj);
	return ret;
}

static long thread_sync_sleep_wakeup(long *addr, int idx)
{
	objid_t id;
	uint64_t off;
	if(!vm_vaddr_lookup(addr, &id, &off)) {
		return -1; /* TODO (major): err codes in all syscalls */
	}
	struct object *obj = obj_lookup(id, 0);
	if(!obj) {
		return -1;
	}
	struct syncpoint *sp = sp_lookup(obj, off, true);
	obj_put(obj);
	sp_sleep_finish(sp, 0, idx);
	return 0;
}

long thread_wake_object(struct object *obj, size_t offset, long arg)
{
	struct syncpoint *sp = sp_lookup(obj, offset, false);
	return sp_wake(sp, arg);
}

static void __thread_init_sync(size_t count)
{
	if(!current_thread->sleep_entries) {
		current_thread->sleep_entries = kcalloc(count, sizeof(struct sleep_entry));
		current_thread->sleep_count = count;
	}
	if(count > current_thread->sleep_count) {
		current_thread->sleep_entries =
		  krecalloc(current_thread->sleep_entries, count, sizeof(struct sleep_entry));
		current_thread->sleep_count = count;
	}
}

long thread_sync_single(int operation, long *addr, long arg, struct timespec *spec)
{
	objid_t id;
	uint64_t off;
	if(!vm_vaddr_lookup(addr, &id, &off)) {
		return -1; /* TODO (major): err codes in all syscalls */
	}
	struct object *obj = obj_lookup(id, 0);
	if(!obj) {
		return -1;
	}
	struct syncpoint *sp = sp_lookup(obj, off, operation == THREAD_SYNC_SLEEP);
	obj_put(obj);
	switch(operation) {
		case THREAD_SYNC_SLEEP:
			__thread_init_sync(1);
			return sp_sleep(sp, addr, arg, spec, 0);
		case THREAD_SYNC_WAKE:
			return sp_wake(sp, arg);
		default:
			break;
	}
	return -1;
}

long syscall_thread_sync(size_t count, struct sys_thread_sync_args *args)
{
	bool ok = false;
	bool wake = false;
	if(count > MAX_SLEEPS)
		return -EINVAL;
	__thread_init_sync(count);
	for(size_t i = 0; i < count; i++) {
		int r = thread_sync_single_norestore(args[i].op,
		  (long *)args[i].addr,
		  args[i].arg,
		  (args[i].flags & THREAD_SYNC_TIMEOUT) ? args[i].spec : NULL,
		  i);
		ok = ok || r >= 0;
		args[i].res = 1; // TODO
		if(args[i].op == THREAD_SYNC_SLEEP && r == 0)
			wake = true;
	}
	if(wake) {
		for(size_t i = 0; i < count; i++) {
			if(args[i].op == THREAD_SYNC_SLEEP)
				thread_sync_sleep_wakeup((long *)args[i].addr, i);
		}
	}
	return ok ? 0 : -1;
}
