#pragma once

#include <lib/linkedlist.h>
#include <spinlock.h>

#define SLAB_OBJ_CREATED   1
#define SLAB_OBJ_ALLOCATED 2

struct slab_object {
	struct linkedentry entry;
	void *obj;
	int flags;
};

struct slab_allocator;
struct slab {
	struct linkedentry entry;
	struct slab_allocator *alloc;
	_Atomic size_t count;
	struct spinlock lock;
	struct linkedlist list;
	struct slab_object objects[];
};

struct slab_allocator {
	bool initialized;
	const size_t num_per_slab;
	const size_t size;
	struct linkedlist full, partial, empty;
	struct spinlock lock;
	void (*const init)(void *);
	void (*const create)(void *);
	void (*const release)(void *);
	void (*const destroy)(void *);
};

#define SLAB_ALLOCATOR(s,n,c,i,r,d) \
	/*(struct slab_allocator)*/ { \
		.init = i, .create = c, .release = r, .destroy = d, .size = (s + sizeof(struct slab *) + 16) & ~15, .num_per_slab = n, .initialized = false, .lock = SPINLOCK_INIT \
	}

void *slab_alloc(struct slab_allocator *alloc);
void slab_release(void *obj);
