#pragma once
#include <machine/memory.h>
#include <string.h>
#include <lib/linkedlist.h>
#define MM_BUDDY_MIN_SIZE 0x1000

struct memregion {
	uintptr_t start;
	size_t length;
	int flags;
	struct linkedentry entry;
};

void mm_init(void);
uintptr_t pmm_buddy_allocate(size_t length);
void pmm_buddy_deallocate(uintptr_t address);
void pmm_buddy_init(void);
struct linkedlist *arch_mm_get_regions(void);

bool      arch_mm_virtual_map(uintptr_t virt, uintptr_t phys, size_t size, int flags);
uintptr_t arch_mm_virtual_unmap(uintptr_t virt);
bool      arch_mm_virtual_getmap(uintptr_t virt, uintptr_t *phys, size_t *size, int *flags);
bool      arch_mm_virtual_chmap(uintptr_t virt, int flags);

static inline uintptr_t mm_physical_alloc(size_t size, bool clear)
{
	size = __round_up_pow2(size);
	uintptr_t ret = pmm_buddy_allocate(size);
	if(clear) memset((void *)(ret + PHYSICAL_MAP_START), 0, size);
	return ret;
}

static inline void mm_physical_dealloc(uintptr_t addr)
{
	pmm_buddy_deallocate(addr);
}

static inline uintptr_t mm_virtual_alloc(size_t size, bool clear)
{
	return mm_physical_alloc(size, clear) + PHYSICAL_MAP_START;
}

static inline void mm_virtual_dealloc(uintptr_t addr)
{
	mm_physical_dealloc(addr - PHYSICAL_MAP_START);
}

