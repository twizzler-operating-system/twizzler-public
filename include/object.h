#pragma once

#include <lib/inthash.h>
#include <spinlock.h>

struct object {
	uint128_t id;
	size_t maxsz;

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

struct object *obj_create(uint128_t id);
struct object *obj_lookup(uint128_t id);
void obj_alloc_slot(struct object *obj);
struct object *obj_lookup_slot(uintptr_t oaddr);
void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys);

#define OBJSPACE_FAULT_READ   1
#define OBJSPACE_FAULT_WRITE  2
#define OBJSPACE_FAULT_EXEC   4

#define OBJSPACE_READ   1
#define OBJSPACE_WRITE  2
#define OBJSPACE_EXEC_U 4
#define OBJSPACE_EXEC_S 8

void kernel_objspace_fault_entry(uintptr_t phys, uint32_t flags);

