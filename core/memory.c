#include <debug.h>
#include <lib/iter.h>
#include <memory.h>
#include <thread.h>
static DECLARE_LIST(physical_regions);
static DECLARE_LIST(physical_regions_alloc);

static const char *memory_type_strings[] = {
	[MEMORY_AVAILABLE] = "System RAM",
	[MEMORY_RESERVED] = "Reserved",
	[MEMORY_CODE] = "Firmware Code",
	[MEMORY_BAD] = "Bad Memory",
	[MEMORY_RECLAIMABLE] = "Reclaimable System Memory",
	[MEMORY_UNKNOWN] = "Unknown Memory",
};

static const char *memory_subtype_strings[] = {
	[MEMORY_SUBTYPE_NONE] = "",
	[MEMORY_AVAILABLE_VOLATILE] = "(volatile)",
	[MEMORY_AVAILABLE_PERSISTENT] = "(persistent)",
};

void mm_register_region(struct memregion *reg, struct mem_allocator *alloc)
{
	reg->alloc = alloc;
	printk("[mm] registering memory region %lx -> %lx %s %s\n",
	  reg->start,
	  reg->start + reg->length - 1,
	  memory_type_strings[reg->type],
	  memory_subtype_strings[reg->subtype]);

	if(alloc) {
		pmm_buddy_init(reg);
		list_insert(&physical_regions_alloc, &reg->alloc_entry);
	}
	list_insert(&physical_regions, &reg->entry);
}

void mm_init_region(struct memregion *reg,
  uintptr_t start,
  size_t len,
  enum memory_type type,
  enum memory_subtype st)
{
	reg->start = start;
	reg->length = len;
	reg->flags = 0;
	reg->alloc = NULL;
	reg->type = type;
	reg->subtype = st;
}

void mm_init(void)
{
	arch_mm_init();
#if 0
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
#endif
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
	foreach(e, list, &physical_regions_alloc) {
		struct memregion *reg = list_entry(e, struct memregion, alloc_entry);

		if((reg->flags & type) == reg->flags && reg->alloc && reg->alloc->free_memory > 0) {
			uintptr_t alloc = mm_physical_region_alloc(reg, length, clear);
			if(alloc)
				return alloc;
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
