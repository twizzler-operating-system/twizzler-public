#pragma once
#include <stdatomic.h>
#include <debug.h>

struct refcalls {
	void (*put)(void *);
	void (*init)(void *);
};

#define REFCALLS(i,p) \
	(struct refcalls) { .put = p, .init = i }

struct ref {
	_Atomic unsigned long count;
	void *obj;
	struct refcalls *calls;
};

static inline void ref_init(struct ref *ref, void *obj, struct refcalls *calls)
{
	ref->calls = calls;
	ref->obj = obj;
	ref->count = 1;
	if(calls->init)
		calls->init(obj);
}

static inline void *ref_get(struct ref *ref)
{
	assert(ref->count > 0);
	atomic_fetch_add(&ref->count, 1);
	return ref->obj;
}

static inline void ref_put(struct ref *ref)
{
	assert(ref->count > 0);
	if(atomic_fetch_sub(&ref->count, 1) == 1) {
		if(ref->calls->put)
			ref->calls->put(ref->obj);
	}
}

static inline bool ref_try_get_nonzero(struct ref *ref)
{
	unsigned long c = ref->count, n;
	do {
		if(c == 0)
			return false;
		n = c + 1;
	} while(!atomic_compare_exchange_weak(&ref->count, &c, n));
	return true;
}

