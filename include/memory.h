#pragma once
#include <arch/memory.h>
#include <lib/list.h>
#include <machine/memory.h>
#include <spinlock.h>
#include <string.h>

#define MM_BUDDY_MIN_SIZE 0x1000
#define MAX_ORDER 20
#define MIN_SIZE MM_BUDDY_MIN_SIZE
#define MAX_SIZE ((uintptr_t)MIN_SIZE << MAX_ORDER)

#define PM_TYPE_NV 1
#define PM_TYPE_DRAM 2
#define PM_TYPE_ANY (~0)

enum memory_type {
	MEMORY_UNKNOWN,
	MEMORY_AVAILABLE,
	MEMORY_RESERVED,
	MEMORY_RECLAIMABLE,
	MEMORY_BAD,
	MEMORY_CODE,
	MEMORY_KERNEL_IMAGE,
};

enum memory_subtype {
	MEMORY_SUBTYPE_NONE,
	MEMORY_AVAILABLE_VOLATILE,
	MEMORY_AVAILABLE_PERSISTENT,
};

struct mem_allocator {
	struct spinlock pm_buddy_lock;
	uint8_t *bitmaps[MAX_ORDER + 1];
	struct list freelists[MAX_ORDER + 1];
	size_t num_allocated[MAX_ORDER + 1];
	_Atomic size_t free_memory;
	bool ready;
	// char static_bitmaps[((MEMORY_SIZE / MIN_SIZE) / 8) * 2];
	char *static_bitmaps;
	size_t off;
};

struct memregion {
	uintptr_t start;
	size_t length;
	int flags;
	enum memory_type type;
	enum memory_subtype subtype;
	struct mem_allocator *alloc;
	struct list entry, alloc_entry;
	size_t off;
};

void mm_init(void);
void arch_mm_init(void);
uintptr_t pmm_buddy_allocate(struct memregion *, size_t length);
void pmm_buddy_deallocate(struct memregion *, uintptr_t address);
void pmm_buddy_init(struct memregion *);
void mm_register_region(struct memregion *reg, struct mem_allocator *alloc);
void mm_init_region(struct memregion *reg,
  uintptr_t start,
  size_t len,
  enum memory_type type,
  enum memory_subtype);

uintptr_t mm_physical_alloc(size_t length, int type, bool clear);
struct memregion *mm_physical_find_region(uintptr_t addr);
void mm_physical_dealloc(uintptr_t addr);

static inline uintptr_t mm_physical_region_alloc(struct memregion *r, size_t size, bool clear)
{
	size = __round_up_pow2(size);
	uintptr_t ret = pmm_buddy_allocate(r, size);
	if(clear)
		memset(mm_ptov(ret), 0, size);
	return ret;
}

static inline void mm_physical_region_dealloc(struct memregion *r, uintptr_t addr)
{
	pmm_buddy_deallocate(r, addr);
}

static inline uintptr_t mm_virtual_region_alloc(struct memregion *r, size_t size, bool clear)
{
	return (uintptr_t)mm_ptov(mm_physical_region_alloc(r, size, clear));
}

static inline void mm_virtual_region_dealloc(struct memregion *r, uintptr_t addr)
{
	mm_physical_region_dealloc(r, mm_vtop((void *)addr));
}

static inline uintptr_t mm_virtual_alloc(size_t size, int type, bool clear)
{
	return (uintptr_t)mm_ptov(mm_physical_alloc(size, type, clear));
}

static inline void mm_virtual_dealloc(uintptr_t addr)
{
	mm_physical_dealloc(mm_vtop((void *)addr));
}

#include <lib/rb.h>
#include <twz/_view.h>

struct vm_context {
	struct arch_vm_context arch;
	struct kso_view *view;
	struct rbroot root;
	struct spinlock lock;
};

void arch_mm_switch_context(struct vm_context *vm);
void arch_mm_context_init(struct vm_context *ctx);

struct vmap {
	struct object *obj;
	size_t slot;
	uint32_t flags;
	int status;

	struct rbnode node;
};

// struct vmap *vm_context_map(struct vm_context *v, uint128_t objid, size_t slot, uint32_t flags);
void vm_context_destroy(struct vm_context *v);
struct vm_context *vm_context_create(void);
void kso_view_write(struct object *obj, size_t slot, struct viewentry *v);
void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags);
struct object;
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct object *obj);
void arch_vm_unmap_object(struct vm_context *ctx, struct vmap *map, struct object *obj);
bool arch_vm_map(struct vm_context *ctx, uintptr_t virt, uintptr_t phys, int level, uint64_t flags);
bool arch_vm_unmap(struct vm_context *ctx, uintptr_t virt);
bool vm_map_contig(struct vm_context *v,
  uintptr_t virt,
  uintptr_t phys,
  size_t len,
  uintptr_t flags);
struct thread;
bool vm_setview(struct thread *, struct object *viewobj);
bool vm_vaddr_lookup(void *addr, objid_t *id, uint64_t *off);

#define FAULT_EXEC 0x1
#define FAULT_WRITE 0x2
#define FAULT_USER 0x4
#define FAULT_ERROR_PERM 0x10
#define FAULT_ERROR_PRES 0x20
void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags);
