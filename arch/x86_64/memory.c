#include <memory.h>

static struct memregion _memregion;
extern uint64_t x86_64_top_mem;
extern uint64_t x86_64_bot_mem;
extern int kernel_end;
#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)
void arch_mm_get_regions(struct list *list)
{
	_memregion.start = ((x86_64_bot_mem - 1) & ~(0x1000 - 1)) + 0x1000;
	_memregion.length = x86_64_top_mem - _memregion.start;
	_memregion.flags = 0;
	list_insert(list, &_memregion.entry);
}

