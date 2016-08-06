#include <memory.h>
#include <lib/linkedlist.h>

struct riscv_meminfo {
	uint64_t base;
	uint64_t size;
	uint64_t node_id;
};

static struct linkedlist _mem_region_list;
static struct memregion _memregion;
extern struct riscv_meminfo riscv_meminfo;
extern uint64_t start_phys_free;
struct linkedlist *arch_mm_get_regions(void)
{
	linkedlist_create(&_mem_region_list, 0);

	_memregion.start = start_phys_free;
	_memregion.length = riscv_meminfo.size - (start_phys_free - riscv_meminfo.base);
	_memregion.flags = 0;
	linkedlist_insert(&_mem_region_list, &_memregion.entry, &_memregion);

	return &_mem_region_list;
}


