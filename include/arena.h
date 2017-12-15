#pragma once

#include <spinlock.h>
#include <memory.h>
#include <debug.h>
#include <system.h>

struct arena {
	void *start;
	struct spinlock lock;
};

struct arena_node {
	size_t used;
	size_t len;
	void *next;
};

static inline void arena_create(struct arena *arena)
{
	arena->lock = SPINLOCK_INIT;
	arena->start = (void *)mm_virtual_alloc(arch_mm_page_size(0), PM_TYPE_ANY, true);
	struct arena_node *node = arena->start;
	node->len = arch_mm_page_size(0);
	node->used = sizeof(struct arena_node);
}

/* TODO: this could be optimized by saving the last-allocated pointer to start from instead
 * of starting from the beginning each time. */
static inline void *arena_allocate(struct arena *arena, size_t length)
{
	bool fl = spinlock_acquire(&arena->lock);
	
	length = (length & ~15) + 16;

	struct arena_node *node = arena->start, *prev = NULL;
	while(node && (node->used + length >= node->len)) {
		prev = node;
		node = node->next;
	}

	if(!node) {
		assert(prev != NULL);
		size_t len = __round_up_pow2(length * 2);
		if(len < arch_mm_page_size(0))
			len = arch_mm_page_size(0);
		node = prev->next = (void *)mm_virtual_alloc(len, PM_TYPE_ANY, true);
		node->len = len;
		node->used = sizeof(struct arena_node);
	}
	
	void *ret = (void *)((uintptr_t)node + node->used);
	node->used += length;

	spinlock_release(&arena->lock, fl);
	return ret;
}

static inline void arena_destroy(struct arena *arena)
{
	struct arena_node *node = arena->start, *next;
	while(node) {
		next = node->next;
		mm_virtual_dealloc((uintptr_t)node);
		node = next;
	}
}

