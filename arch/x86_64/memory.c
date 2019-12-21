#include <memory.h>

static struct memregion _memregion;
extern uint64_t x86_64_top_mem;
extern uint64_t x86_64_bot_mem;
extern int kernel_end;
#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x)-PHYS_ADDR_DELTA)
void arch_mm_get_regions(struct list *list)
{
	_memregion.start = ((x86_64_bot_mem - 1) & ~(0x1000 - 1)) + 0x1000;
	_memregion.length = x86_64_top_mem - _memregion.start;
	_memregion.flags = 0;
	list_insert(list, &_memregion.entry);
}

#define MAX_REGIONS 32
static struct memregion _regions[MAX_REGIONS] = {};
static struct mem_allocator _allocators[MAX_REGIONS] = {};
static int _region_counter = 0;

void arch_mm_init(void)
{
	for(int i = 0; i < _region_counter; i++) {
		struct memregion *reg = &_regions[i];
		mm_register_region(reg,
		  reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_VOLATILE
		    ? &_allocators[i]
		    : NULL);
	}
}

void x86_64_memory_record(uintptr_t addr, size_t len, enum memory_type type, enum memory_subtype st)
{
	// printk("memory region %llx %llx %d %d\n", addr, len, type, st);
	// printk(":::: %ld %ld\n", sizeof(struct memregion), sizeof(_allocators[0]));
	mm_init_region(&_regions[_region_counter++], addr, len, type, st);
}
