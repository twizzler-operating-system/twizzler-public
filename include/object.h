#pragma once

#include <arch/object.h>
#include <krc.h>
#include <lib/inthash.h>
#include <lib/rb.h>
#include <spinlock.h>
#include <twz/_kso.h>
#include <twz/_obj.h>
#include <workqueue.h>

void obj_print_stats(void);

struct kso_view {
};

struct kso_throbj {
	struct thread *thread;
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

#define kso_get_obj(ptr, type)                                                                     \
	({                                                                                             \
		struct object *_o = container_of(ptr, struct object, type);                                \
		krc_get(&_o->refs);                                                                        \
		_o;                                                                                        \
	})

struct object;
struct thread;
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
int kso_root_attach(struct object *obj, uint64_t flags, int type);
void kso_root_detach(int idx);
void kso_attach(struct object *parent, struct object *child, size_t);
void kso_setname(struct object *obj, const char *name);
struct object *get_system_object(void);

struct slot;
struct vmap;

struct object_tie {
	struct object *child;
	struct rbnode node;
	size_t count;
};

#define OF_PINNED 1
#define OF_IDCACHED 2
#define OF_IDSAFE 4
#define OF_KERNEL 8
#define OF_PERSIST 0x10
#define OF_ALLOC 0x20
#define OF_CPF_VALID 0x40
#define OF_DELETE 0x80
#define OF_HIDDEN 0x100
#define OF_PAGER 0x200
#define OF_PARTIAL 0x400

struct derivation_info {
	objid_t id;
	struct list entry;
};

struct object {
	uint128_t id;
	struct arch_object arch;

	struct krc refs, mapcount, kaddr_count;

	struct slot *slot;

	_Atomic uint64_t flags;

	uint32_t cache_mode;
	uint32_t cached_pflags;

	/* KSO stuff: what type, some data needed by each type, and callbacks */
	_Atomic enum kso_type kso_type;
	union {
		struct kso_view view;
		struct kso_throbj thr;
		struct kso_sctx sctx;
		void *data;
	};
	struct kso_calls *kso_calls;
	long (*kaction)(struct object *, long, long);

	/* general object lock */
	struct spinlock lock;

	/* lock for thread-sync operations */
	struct spinlock tslock;

	struct spinlock sleepers_lock;
	struct list sleepers;

	struct rbroot pagecache_root, pagecache_level1_root, tstable_root, page_requests_root;

	struct rbnode slotnode, node;

	struct rbroot ties_root;

	struct task delete_task;

	/* needed for kernel to access objects */
	struct vmap *kvmap;
	struct slot *kslot;
	void *kaddr;

	struct object *sourced_from;
	struct list derivations;
	struct rbroot idx_map;
};

#define obj_get_kbase(obj) ({ (void *)((char *)obj_get_kaddr(obj) + OBJ_NULLPAGE_SIZE); })

struct page;
#define OBJPAGE_MAPPED 1
#define OBJPAGE_COW 2
struct objpage {
	size_t idx, srcidx;
	uint64_t flags;
	struct page *page;
	struct krc refs;
	struct rbnode node, idx_map_node;
	struct spinlock lock;
	struct object *obj; /* weak */
};

struct object_space {
	struct arch_object_space arch;
	struct krc refs;
};

void object_space_map_slot(struct object_space *space, struct slot *slot, uint64_t flags);
void object_space_release_slot(struct slot *slot);

struct object *obj_create(uint128_t id, enum kso_type);
void obj_system_init(void);
void obj_system_init_objpage(void);
struct object *obj_create_clone(uint128_t id, objid_t srcid, enum kso_type ksot);

#define OBJ_LOOKUP_HIDDEN 1
struct object *obj_lookup(uint128_t id, int flags);

bool obj_verify_id(struct object *obj, bool cache_result, bool uncache);
struct slot *obj_alloc_slot(struct object *obj);
void obj_alloc_kernel_slot(struct object *obj);
struct object *obj_lookup_slot(uintptr_t oaddr, struct slot **);
void obj_cache_page(struct object *obj, size_t idx, struct page *);
void obj_kso_init(struct object *obj, enum kso_type ksot);
void obj_put_page(struct objpage *p);

enum obj_get_page_result {
	GETPAGE_OK,
	GETPAGE_PAGER,
	GETPAGE_NOENT,
};

#define OBJ_GET_PAGE_PAGEROK 1
#define OBJ_GET_PAGE_ALLOC 2

enum obj_get_page_result obj_get_page(struct object *obj, size_t idx, struct objpage **, int);
void obj_put(struct object *o);
void obj_assign_id(struct object *obj, objid_t id);
objid_t obj_compute_id(struct object *obj);
void obj_init(struct object *obj);
void obj_system_init(void);
void obj_release_slot(struct object *obj);
void obj_tie(struct object *, struct object *);
int obj_untie(struct object *parent, struct object *child);

bool obj_kaddr_valid(struct object *obj, void *kaddr, size_t);
void obj_release_kaddr(struct object *obj);
void *obj_get_kaddr(struct object *obj);
void obj_copy_pages(struct object *dest, struct object *src, size_t doff, size_t soff, size_t len);

#define OBJPAGE_RELEASE_UNMAP 1
#define OBJPAGE_RELEASE_OBJLOCKED 2
void objpage_release(struct objpage *op, int);

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr);
void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val);
bool obj_get_pflags(struct object *obj, uint32_t *pf);
int obj_check_permission(struct object *obj, uint64_t flags);
void obj_free_kaddr(struct object *obj);

struct slot;
void arch_object_map_slot(struct object_space *, struct object *obj, struct slot *, uint64_t flags);
void arch_object_unmap_slot(struct object_space *space, struct slot *slot);
void arch_object_unmap_page(struct object *obj, size_t idx);
void arch_object_unmap_all(struct object *obj);
bool arch_object_map_page(struct object *obj, struct objpage *);
bool arch_object_map_flush(struct object *obj, size_t idx);
bool arch_object_premap_page(struct object *obj, int idx, int level);
void arch_object_page_remap_cow(struct objpage *op);
bool arch_object_getmap(struct object *obj,
  uintptr_t off,
  uintptr_t *phys,
  int *level,
  uint64_t *flags);
void arch_object_remap_cow(struct object *obj);

void arch_object_space_init(struct object_space *space);
void arch_object_space_destroy(struct object_space *space);
bool arch_object_getmap_slot_flags(struct object_space *space, struct slot *, uint64_t *flags);
void object_space_destroy(struct object_space *space);
void object_space_init(struct object_space *space);
void arch_object_init(struct object *obj);
void arch_object_destroy(struct object *obj);

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

void obj_clone_cow(struct object *src, struct object *nobj);
