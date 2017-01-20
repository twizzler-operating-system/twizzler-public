#include <memory.h>
static struct linkedlist _mem_region_list;
static struct memregion _memregion;
extern uint64_t x86_64_top_mem;
extern int kernel_end;
#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)
struct linkedlist *arch_mm_get_regions(void)
{
	linkedlist_create(&_mem_region_list, 0);

	_memregion.start = PHYS((uint64_t)&kernel_end);
	_memregion.length = x86_64_top_mem - _memregion.start;
	_memregion.flags = 0;
	linkedlist_insert(&_mem_region_list, &_memregion.entry, &_memregion);

	return &_mem_region_list;
}

