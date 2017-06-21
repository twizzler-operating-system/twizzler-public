#include <memory.h>
#include <debug.h>
#include <thread.h>
static struct linkedlist *physical_regions;

void mm_init(void)
{
	physical_regions = arch_mm_get_regions();
	for(struct linkedentry *entry = linkedlist_iter_start(physical_regions);
			entry != linkedlist_iter_end(physical_regions);
			entry = linkedlist_iter_next(entry)) {
		struct memregion *reg = linkedentry_obj(entry);
		pmm_buddy_init(reg);

		printk("[mm]: memory region %lx -> %lx (%ld KB), %x\n",
				reg->start, reg->start + reg->length, reg->length / 1024, reg->flags);

		for(uintptr_t addr = reg->start; addr < reg->start + reg->length;
				addr += MM_BUDDY_MIN_SIZE) {
			pmm_buddy_deallocate(reg, addr);
		}
	}
}

struct memregion *mm_physical_find_region(uintptr_t addr)
{
	for(struct linkedentry *entry = linkedlist_iter_start(physical_regions);
			entry != linkedlist_iter_end(physical_regions);
			entry = linkedlist_iter_next(entry)) {
		struct memregion *reg = linkedentry_obj(entry);
		if(addr >= reg->start && addr < reg->start + reg->length)
			return reg;
	}
	return NULL;
}

uintptr_t mm_physical_alloc(size_t length, int type, bool clear)
{
	physical_regions = arch_mm_get_regions();
	for(struct linkedentry *entry = linkedlist_iter_start(physical_regions);
			entry != linkedlist_iter_end(physical_regions);
			entry = linkedlist_iter_next(entry)) {
		struct memregion *reg = linkedentry_obj(entry);

		if((reg->flags & type) == reg->flags && reg->free_memory > 0) {
			/* TODO: if this fails, keep trying on a different region */
			return mm_physical_region_alloc(reg, length, clear);
		}
	}
	return 0;
}

void mm_physical_dealloc(uintptr_t addr)
{
	struct memregion *reg = mm_physical_find_region(addr);
	assert(reg != NULL);

	mm_physical_region_dealloc(reg, addr);
}

void kernel_fault_entry(uintptr_t addr, int flags)
{
	if(addr < KERNEL_VIRTUAL_BASE) {
		vm_context_fault(addr, flags);
	} else {
		panic("kernel page fault: %lx, %x", addr, flags);
	}
}

