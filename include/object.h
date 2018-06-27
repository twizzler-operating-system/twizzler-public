#pragma once

#include <lib/inthash.h>
#include <spinlock.h>

enum kso_type {
	KSO_NONE,
	KSO_VIEW,
	KSO_SECCTX,
	KSO_THREAD,
	KSO_ROOT,
};

#define OF_NOTYPECHECK 1

struct kso_view {

};

struct kso_throbj {

};

#define kso_get_obj(ptr, type) \
	container_of(ptr, struct object, type)

struct object {
	uint128_t id;
	size_t maxsz;

	int pglevel;
	int flags;
	ssize_t slot;

	enum kso_type kso_type;
	union {
		struct kso_view view;
		struct kso_throbj thr;
	};

	struct spinlock lock;
	struct ihtable *pagecache;

	struct ihelem elem, slotelem;
};

struct objpage {
	size_t idx;
	uintptr_t phys;
	struct ihelem elem;
};

struct object *obj_create(uint128_t id, enum kso_type);
struct object *obj_lookup(uint128_t id);
void obj_alloc_slot(struct object *obj);
struct object *obj_lookup_slot(uintptr_t oaddr);
void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys);


void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr);

#define OBJSPACE_FAULT_READ   1
#define OBJSPACE_FAULT_WRITE  2
#define OBJSPACE_FAULT_EXEC   4

#define OBJSPACE_READ   1
#define OBJSPACE_WRITE  2
#define OBJSPACE_EXEC_U 4
#define OBJSPACE_EXEC_S 8

void kernel_objspace_fault_entry(uintptr_t phys, uint32_t flags);

