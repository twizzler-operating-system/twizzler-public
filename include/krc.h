#pragma once

#include <debug.h>
#include <stdatomic.h>
#include <processor.h>

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

static inline bool krc_put_call(struct krc *k, void (*_fn)(struct krc *k))
{
	assert(k->count > 0);
	bool r = atomic_fetch_sub(&k->count, 1) == 1;
	if(r) {
		_fn(k);
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

