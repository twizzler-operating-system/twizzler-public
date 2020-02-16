#include <memory.h>

extern uint64_t x86_64_top_mem;
extern uint64_t x86_64_bot_mem;

#define MAX_REGIONS 32
static struct memregion _regions[MAX_REGIONS] = {};
static int _region_counter = 0;

void arch_mm_init(void)
{
	for(int i = 0; i < _region_counter; i++) {
		struct memregion *reg = &_regions[i];
		mm_register_region(reg);
	}
}

void x86_64_memory_record(uintptr_t addr, size_t len, enum memory_type type, enum memory_subtype st)
{
	size_t off = 0;
	if(addr < 1024 * 1024) {
		/* keep the lower 1MB untouched */
		off = 1024 * 1024 - addr;
	}
	if(off >= len)
		return;
	len -= off;
	addr += off;
	mm_init_region(&_regions[_region_counter++], addr, len, type, st);
}

static struct memregion kernel_region, initrd_region;

void x86_64_register_kernel_region(uintptr_t addr, size_t len)
{
	mm_init_region(&kernel_region, addr, len, MEMORY_KERNEL_IMAGE, MEMORY_SUBTYPE_NONE);
	mm_register_region(&kernel_region);
}

void x86_64_register_initrd_region(uintptr_t addr, size_t len)
{
	mm_init_region(&initrd_region, addr, len, MEMORY_AVAILABLE, MEMORY_AVAILABLE_VOLATILE);
}

void x86_64_reclaim_initrd_region(void)
{
	mm_register_region(&initrd_region);
}
