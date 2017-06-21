#pragma once
#include <machine/memory.h>
#include <arch/memory.h>
#include <string.h>
#include <lib/linkedlist.h>

#define MM_BUDDY_MIN_SIZE 0x1000
#define MAX_ORDER 20
#define MIN_SIZE MM_BUDDY_MIN_SIZE
#define MAX_SIZE ((uintptr_t)MIN_SIZE << MAX_ORDER)
#define MEMORY_SIZE (MAX_SIZE)

#define PM_TYPE_NV   1
#define PM_TYPE_DRAM 2
#define PM_TYPE_ANY  (~0)

struct memregion {
	uintptr_t start;
	size_t length;
	int flags;

	struct spinlock pm_buddy_lock;
	uint8_t *bitmaps[MAX_ORDER + 1];
	struct linkedlist freelists[MAX_ORDER+1];
	char static_bitmaps[((MEMORY_SIZE / MIN_SIZE) / 8) * 2];
	size_t num_allocated[MAX_ORDER + 1];
	_Atomic size_t free_memory;

	struct linkedentry entry;
};

void mm_init(void);
uintptr_t pmm_buddy_allocate(struct memregion *, size_t length);
void pmm_buddy_deallocate(struct memregion *, uintptr_t address);
void pmm_buddy_init(struct memregion *);
struct linkedlist *arch_mm_get_regions(void);


uintptr_t mm_physical_alloc(size_t length, int type, bool clear);
struct memregion *mm_physical_find_region(uintptr_t addr);
void mm_physical_dealloc(uintptr_t addr);

static inline uintptr_t mm_physical_region_alloc(struct memregion *r, size_t size, bool clear)
{
	size = __round_up_pow2(size);
	uintptr_t ret = pmm_buddy_allocate(r, size);
	if(clear) memset((void *)(ret + PHYSICAL_MAP_START), 0, size);
	return ret;
}

static inline void mm_physical_region_dealloc(struct memregion *r, uintptr_t addr)
{
	pmm_buddy_deallocate(r, addr);
}

static inline uintptr_t mm_virtual_region_alloc(struct memregion *r, size_t size, bool clear)
{
	return mm_physical_region_alloc(r, size, clear) + PHYSICAL_MAP_START;
}

static inline void mm_virtual_region_dealloc(struct memregion *r, uintptr_t addr)
{
	mm_physical_region_dealloc(r, addr - PHYSICAL_MAP_START);
}

static inline uintptr_t mm_virtual_alloc(size_t size, int type, bool clear)
{
	return mm_physical_alloc(size, type, clear) + PHYSICAL_MAP_START;
}

static inline void mm_virtual_dealloc(uintptr_t addr)
{
	mm_physical_dealloc(addr - PHYSICAL_MAP_START);
}


struct vm_context {
	struct arch_vm_context arch;
	struct ihtable *maps;
};

void arch_mm_switch_context(struct vm_context *vm);
void arch_mm_context_init(struct vm_context *ctx);

#include <lib/inthash.h>
struct vmap {
	uint128_t target;
	size_t slot;
	uint32_t flags;
	int status;

	struct ihelem elem;
};

#define VMAP_NONE  0
#define VMAP_READ  1
#define VMAP_WRITE 2
#define VMAP_EXEC  4

int vm_context_map(struct vm_context *v, uint128_t objid, size_t slot, uint32_t flags);
void vm_context_destroy(struct vm_context *v);
struct vm_context *vm_context_create(void);
void vm_context_fault(uintptr_t addr, int flags);
struct object;
void arch_vm_map_object(struct vm_context *ctx, struct vmap *map, struct object *obj);

#define FAULT_EXEC  0x1
#define FAULT_WRITE 0x2
#define FAULT_USER  0x4
#define FAULT_ERROR_PERM 0x10
#define FAULT_ERROR_PRES 0x20
void kernel_fault_entry(uintptr_t addr, int flags);

