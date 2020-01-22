#pragma once

#include <debug.h>
#include <spinlock.h>
#include <stdatomic.h>

struct krc {
	_Atomic int64_t count;
};

static inline void krc_init(struct krc *k)
{
	k->count = 1;
}

static inline void krc_get(struct krc *k)
{
	atomic_fetch_add(&k->count, 1);
}

static inline bool krc_get_unless_zero(struct krc *k)
{
	int64_t c = k->count;
	while(true) {
		if(unlikely(c == 0)) {
			return false;
		}
		if(likely(atomic_compare_exchange_weak(&k->count, &c, c + 1))) {
			return true;
		}
	}
}

static inline bool krc_put(struct krc *k)
{
	assert(k->count > 0);
	return atomic_fetch_sub(&k->count, 1) == 1;
}

#define krc_put_call(o, k_mname, fn) __krc_put_call(&((o)->k_mname), fn, o)

static inline bool __krc_put_call(struct krc *k, void (*_fn)(void *k), void *o)
{
	assert(k->count > 0);
	bool r = atomic_fetch_sub(&k->count, 1) == 1;
	if(r) {
		_fn(o);
	}
	return r;
}

static inline bool krc_put_locked(struct krc *k, struct spinlock *lock)
{
	assert(k->count > 0);

	/* TODO: optimize via dec_if_not_one */

	spinlock_acquire_save(lock);
	bool r = atomic_fetch_sub(&k->count, 1) == 1;
	if(!r) {
		spinlock_release_restore(lock);
	}
	return r;
}

static inline bool krc_iszero(struct krc *k)
{
	return k->count == 0;
}

static inline void krc_init_zero(struct krc *k)
{
	k->count = 0;
}
