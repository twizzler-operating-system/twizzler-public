#include <memory.h>

extern uint64_t x86_64_top_mem;
extern uint64_t x86_64_bot_mem;

#define MAX_REGIONS 32
static struct memregion _regions[MAX_REGIONS] = {};
static struct mem_allocator _allocators[MAX_REGIONS] = {};
static int _region_counter = 0;

void arch_mm_init(void)
{
	if(_region_counter == 0) {
		/* fall-back to old method. TODO: remove this */
		mm_init_region(&_regions[0],
		  x86_64_bot_mem,
		  x86_64_top_mem - x86_64_bot_mem,
		  MEMORY_AVAILABLE,
		  MEMORY_AVAILABLE_VOLATILE);
		mm_register_region(&_regions[0], &_allocators[0]);
		return;
	}
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
	mm_init_region(&_regions[_region_counter++], addr, len, type, st);
}
