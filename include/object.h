#pragma once

#include <arch/object.h>
#include <krc.h>
#include <lib/inthash.h>
#include <secctx.h>
#include <spinlock.h>
#include <twz/_obj.h>

#define OF_NOTYPECHECK 1
#define OF_KERNELGEN 2

struct kso_view {
};

struct kso_throbj {
};

struct kso_sctx {
	struct sctx *sc;
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
	bool (*detach)(struct object *parent, struct object *child, int sysc, int flags);
	bool (*detach_event)(struct thread *thr, bool, int);
	void (*ctor)(struct object *);
	void (*dtor)(struct object *);
	bool (*invl)(struct object *, struct kso_invl_args *);
};

void kso_register(int t, struct kso_calls *);
struct kso_calls *kso_lookup_calls(int t);
void kso_detach_event(struct thread *thr, bool entry, int sysc);
void kso_root_attach(struct object *obj, uint64_t flags, int type);
void kso_attach(struct object *parent, struct object *child, size_t);
void kso_setname(struct object *obj, const char *name);
struct object *get_system_object(void);

struct object {
	uint128_t id;

	size_t maxsz;

	struct krc refs, pcount;

	int pglevel;
	int flags;
	ssize_t slot;
	bool pinned;
	bool idvercache;
	bool idversafe;
	bool persist; // TODO: combine these into flags
	int cache_mode;

	_Atomic enum kso_type kso_type;
	union {
		struct kso_view view;
		struct kso_throbj thr;
		struct kso_sctx sctx;
		void *data;
	};
	struct kso_calls *kso_calls;
	long (*kaction)(struct object *, long, long);

	struct spinlock lock, tslock, verlock;
	struct ihtable *pagecache, *tstable;

	struct ihelem elem, slotelem;
	struct arch_object arch;
};

struct page;
#define OBJPAGE_MAPPED 1
struct objpage {
	size_t idx;
	uint64_t flags;
	struct page *page;
	struct krc refs;
	struct ihelem elem;
};

struct object *obj_create(uint128_t id, enum kso_type);
struct object *obj_create_clone(uint128_t id, objid_t srcid, enum kso_type ksot);
struct object *obj_lookup(uint128_t id);
bool obj_verify_id(struct object *obj, bool cache_result, bool uncache);
void obj_alloc_slot(struct object *obj);
struct object *obj_lookup_slot(uintptr_t oaddr);
void obj_cache_page(struct object *obj, size_t idx, struct page *);
void obj_kso_init(struct object *obj, enum kso_type ksot);
void obj_put_page(struct objpage *p);
struct objpage *obj_get_page(struct object *obj, size_t idx, bool);
void obj_put(struct object *o);
void obj_assign_id(struct object *obj, objid_t id);
objid_t obj_compute_id(struct object *obj);

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val);
int obj_check_permission(struct object *obj, uint64_t flags);

void arch_object_map_slot(struct object *obj, uint64_t flags);
void arch_object_unmap_page(struct object *obj, size_t idx);
bool arch_object_map_page(struct object *obj, struct page *page, size_t idx);
bool arch_object_map_flush(struct object *obj, size_t idx);
bool arch_object_getmap_slot_flags(struct object *obj, uint64_t *flags);
void arch_object_init(struct object *obj);

#define OBJSPACE_FAULT_READ 1
#define OBJSPACE_FAULT_WRITE 2
#define OBJSPACE_FAULT_EXEC 4

#define OBJSPACE_READ 1
#define OBJSPACE_WRITE 2
#define OBJSPACE_EXEC_U 4
#define OBJSPACE_EXEC_S 8
#define OBJSPACE_SET_FLAGS                                                                         \
	0x1000 /* allow changing the permissions of a page, as long as phys matches */
#define OBJSPACE_UC 0x2000
#define OBJSPACE_WB 0
#define OBJSPACE_WT 0x4000
#define OBJSPACE_WC 0x8000
#define OBJSPACE_WP 0x10000

void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t phys, uintptr_t vaddr, uint32_t flags);
bool arch_objspace_getmap(uintptr_t v, uintptr_t *p, int *level, uint64_t *flags);
bool arch_objspace_map(uintptr_t v, uintptr_t p, int level, uint64_t flags);
