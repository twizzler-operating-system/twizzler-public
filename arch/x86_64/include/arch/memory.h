#pragma once

#include <debug.h>
#include <machine/memory.h>

struct arch_vm_context {
	uintptr_t pml4_phys;
	uint64_t *pml4;
	uint64_t **kernel_pdpts;
	uint64_t **user_pdpts;
};

/* Intel 3A 4.5 */
#define VM_MAP_USER (1ull << 2)
#define VM_MAP_WRITE (1ull << 1)
#define VM_MAP_ACCESSED (1ull << 5)
#define VM_MAP_DIRTY (1ull << 6)
#define VM_MAP_GLOBAL (1ull << 8)
#define VM_MAP_DEVICE (1ull << 4)
/* x86 has a no-execute bit, so we'll need to fix this up in the mapping functions */
#define VM_MAP_EXEC (1ull << 63)

#define VM_PHYS_MASK (0x7FFFFFFFFFFFF000)
#define VM_ADDR_SIZE (1ul << 48)
#define MAX_PGLEVEL 2
__attribute__((const)) static inline size_t mm_page_size(int level)
{
	assert(level < 3);
	static const size_t __pagesizes[3] = { 0x1000, 2 * 1024 * 1024, 1024 * 1024 * 1024 };
	return __pagesizes[level];
}

__attribute__((const)) static inline int mm_page_max_level(size_t sz)
{
	if(sz >= (1024 * 1024 * 1024))
		return 2;
	if(sz >= 2 * 1024 * 1024)
		return 1;
	assert(sz >= 0x1000);
	return 0;
}

#define OM_ADDR_SIZE (1ul << 48)

__attribute__((const)) static inline uintptr_t mm_vtop(void *addr)
{
	return (uintptr_t)addr - PHYSICAL_MAP_START;
}

__attribute__((const)) static inline void *mm_ptov(uintptr_t addr)
{
	return (void *)(addr + PHYSICAL_MAP_START);
}
