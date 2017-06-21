#pragma once

#include <spinlock.h>

struct slabcache;
struct slab {
	struct slab *next, *prev;
	uint64_t alloc;
	struct slabcache *slabcache;
	_Alignas(sizeof(void *)) char data[];
};

struct slabcache {
	struct slab empty, partial, full;
	void (*ctor)(void *, void *);
	void (*dtor)(void *, void *);
	size_t sz;
	void *ptr;
	struct spinlock lock;
};


void slabcache_init(struct slabcache *c, size_t sz,
		void (*ctor)(void *, void *), void (*dtor)(void *, void *), void *ptr);
void slabcache_reap(struct slabcache *c);
void slabcache_free(void *obj);
void *slabcache_alloc(struct slabcache *c);

