#pragma once

struct refcalls {
	void (*get)(void *);
	void (*put)(void *);
	void (*init)(void *);
};

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
	if(ref->calls->get)
		ref->calls->get(ref->obj);
}

static inline void ref_put(struct ref *ref)
{
	assert(ref->count > 0);
	if(atomic_fetch_sub(&ref->count, 1) == 1) {
		if(ref->calls->put)
			ref->calls->put(ref->obj);
	}
}

