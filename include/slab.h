#pragma once

#include <spinlock.h>

#define SLAB_CANARY 0x12345678abcdef55

struct slabcache;
struct slab {
	uint64_t canary;
	struct slab *next, *prev;
	unsigned __int128 alloc;
	struct slabcache *slabcache;
	_Alignas(16) char data[];
};

struct slabcache {
	const char *name;
	uint64_t canary;
	struct slab empty, partial, full;
	void (*ctor)(void *, void *);
	void (*dtor)(void *, void *);
	size_t sz;
	void *ptr;
	struct spinlock lock;
	int __cached_nr_obj;
};

#define SLAB_MARKER_MAGIC 0x885522aa

struct slabmarker {
	uint32_t marker_magic;
	uint32_t slot;
	uint64_t pad;
};

#define DECLARE_SLABCACHE(_name, _sz, ct, dt, _pt)                                                 \
	struct slabcache _name = {                                                                     \
		.name = #_name,                                                                            \
		.empty.next = &_name.empty,                                                                \
		.partial.next = &_name.partial,                                                            \
		.full.next = &_name.full,                                                                  \
		.sz = _sz + sizeof(struct slabmarker),                                                     \
		.ctor = ct,                                                                                \
		.dtor = dt,                                                                                \
		.ptr = _pt,                                                                                \
		.lock = SPINLOCK_INIT,                                                                     \
		.canary = SLAB_CANARY,                                                                     \
		.__cached_nr_obj = 0,                                                                      \
	}

void slabcache_init(struct slabcache *c,
  const char *name,
  size_t sz,
  void (*ctor)(void *, void *),
  void (*dtor)(void *, void *),
  void *ptr);
void slabcache_reap(struct slabcache *c);
void slabcache_free(struct slabcache *, void *obj);
void *slabcache_alloc(struct slabcache *c);
