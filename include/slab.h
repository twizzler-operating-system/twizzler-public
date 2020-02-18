#pragma once

#include <spinlock.h>

#define SLAB_CANARY 0x12345678abcdef55

struct slabcache;
struct slab {
	uint64_t canary;
	struct slab *next, *prev;
	uint64_t alloc;
	struct slabcache *slabcache;
	_Alignas(16) char data[];
};

struct slabcache {
	uint64_t canary;
	struct slab empty, partial, full;
	void (*ctor)(void *, void *);
	void (*dtor)(void *, void *);
	size_t sz;
	void *ptr;
	struct spinlock lock;
};

#define DECLARE_SLABCACHE(name, _sz, ct, dt, _pt)                                                  \
	struct slabcache name = {                                                                      \
		.empty.next = &name.empty,                                                                 \
		.partial.next = &name.partial,                                                             \
		.full.next = &name.full,                                                                   \
		.sz = _sz,                                                                                 \
		.ctor = ct,                                                                                \
		.dtor = dt,                                                                                \
		.ptr = _pt,                                                                                \
		.lock = SPINLOCK_INIT,                                                                     \
		.canary = SLAB_CANARY,                                                                     \
	}

void slabcache_init(struct slabcache *c,
  size_t sz,
  void (*ctor)(void *, void *),
  void (*dtor)(void *, void *),
  void *ptr);
void slabcache_reap(struct slabcache *c);
void slabcache_free(void *obj);
void *slabcache_alloc(struct slabcache *c);
