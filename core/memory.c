#include <debug.h>
#include <lib/iter.h>
#include <memory.h>
#include <thread.h>
static DECLARE_LIST(physical_regions);

void mm_init(void)
{
	arch_mm_get_regions(&physical_regions);
	foreach(e, list, &physical_regions) {
		struct memregion *reg = list_entry(e, struct memregion, entry);
		pmm_buddy_init(reg);

		printk("[mm]: memory region %lx -> %lx (%ld MB), %x\n",
		  reg->start,
		  reg->start + reg->length,
		  reg->length / (1024 * 1024),
		  reg->flags);

		for(uintptr_t addr = reg->start; addr < reg->start + reg->length;
		    addr += MM_BUDDY_MIN_SIZE) {
			pmm_buddy_deallocate(reg, addr);
		}
		reg->ready = true;
	}
}

struct memregion *mm_physical_find_region(uintptr_t addr)
{
	foreach(e, list, &physical_regions) {
		struct memregion *reg = list_entry(e, struct memregion, entry);
		if(addr >= reg->start && addr < reg->start + reg->length)
			return reg;
	}
	return NULL;
}

uintptr_t mm_physical_alloc(size_t length, int type, bool clear)
{
	foreach(e, list, &physical_regions) {
		struct memregion *reg = list_entry(e, struct memregion, entry);

		if((reg->flags & type) == reg->flags && reg->free_memory > 0) {
			/* TODO (major): if this fails, keep trying on a different region */
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

void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags)
{
	if(addr < KERNEL_VIRTUAL_BASE) {
		vm_context_fault(ip, addr, flags);
	} else {
		panic("kernel page fault: %lx, %x at ip=%lx", addr, flags, ip);
	}
}
