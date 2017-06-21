#pragma once

#include <lib/inthash.h>
#include <spinlock.h>

struct object {
	uint128_t id;
	size_t maxsz, dataoff;

	int pglevel;
	ssize_t slot;

	struct spinlock lock;
	struct ihtable *pagecache;

	struct ihelem elem, slotelem;
};

struct objpage {
	size_t idx;
	uintptr_t phys;
	struct ihelem elem;
};

void obj_create(uint128_t id, size_t maxsz, size_t dataoff);
struct object *obj_lookup(uint128_t id);
void obj_alloc_slot(struct object *obj);
struct object *obj_lookup_slot(uintptr_t oaddr);
void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys);

