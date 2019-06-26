#pragma once

#include <krc.h>
#include <lib/inthash.h>
#include <spinlock.h>

#include <twz/_obj.h>

enum kso_type {
	KSO_NONE,
	KSO_VIEW,
	KSO_SECCTX,
	KSO_THREAD,
	KSO_ROOT,
	KSO_MAX,
};

#define OF_NOTYPECHECK 1
#define OF_KERNELGEN 2

struct kso_view {
};

struct kso_throbj {
};

struct kso_invl_args {
	objid_t id;
	uint64_t offset;
	uint32_t length;
	uint16_t flags;
	uint16_t result;
};

#define kso_get_obj(ptr, type) container_of(ptr, struct object, type)

struct object;
struct kso_calls {
	bool (*attach)(struct object *parent, struct object *child, int flags);
	bool (*detach)(struct object *parent, struct object *child, int flags);
	void (*ctor)(struct object *);
	void (*dtor)(struct object *);
	bool (*invl)(struct object *, struct kso_invl_args *);
};

void kso_register(int t, struct kso_calls *);

struct object {
	uint128_t id;
	size_t maxsz;

	struct krc refs, pcount;

	int pglevel;
	int flags;
	ssize_t slot;
	bool idvercache;
	bool idversafe;

	enum kso_type kso_type;
	union {
		struct kso_view view;
		struct kso_throbj thr;
	};
	struct kso_calls *kso_calls;

	struct spinlock lock, tslock;
	struct ihtable *pagecache, *tstable;

	struct ihelem elem, slotelem;
};

struct objpage {
	size_t idx;
	uintptr_t phys;
	struct krc refs;
	struct ihelem elem;
};

struct object *obj_create(uint128_t id, enum kso_type);
struct object *obj_create_clone(uint128_t id, objid_t srcid, enum kso_type ksot);
struct object *obj_lookup(uint128_t id);
bool obj_verify_id(struct object *obj, bool cache_result, bool uncache);
void obj_alloc_slot(struct object *obj);
struct object *obj_lookup_slot(uintptr_t oaddr);
void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys);
void obj_kso_init(struct object *obj, enum kso_type ksot);
void obj_put_page(struct objpage *p);
void obj_put(struct object *o);

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr);

#define OBJSPACE_FAULT_READ 1
#define OBJSPACE_FAULT_WRITE 2
#define OBJSPACE_FAULT_EXEC 4

#define OBJSPACE_READ 1
#define OBJSPACE_WRITE 2
#define OBJSPACE_EXEC_U 4
#define OBJSPACE_EXEC_S 8

void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t phys, uint32_t flags);
