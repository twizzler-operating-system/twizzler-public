#pragma once
#include <system.h>
struct _guard {
	void *data;
	void (*fn)();
};

static inline void _guard_release(struct _guard *g)
{
	g->fn(g->data);
}

#define guard_initializer(l,a,r) \
	__cleanup(_guard_release) = (struct _guard) { .data = (a != NULL ? ((void (*)())a)(l) : 0, l), .fn = r }

#define guard(m,a,r) struct _guard __concat(_lg_, __COUNTER__) guard_initializer(m,a,r)

#define defer(f) guard(NULL, NULL, f)

