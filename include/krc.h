#pragma once

#include <debug.h>

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

static inline bool krc_put(struct krc *k)
{
	assert(k->count > 0);
	return atomic_fetch_sub(&k->count, 1) == 1;
}

