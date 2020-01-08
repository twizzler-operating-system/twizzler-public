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

	if(reg->start + reg->length > 32ul * 1024ul * 1024ul * 1024ul) {
		reg->length = 32ul * 1024ul * 1024ul * 1024ul - reg->start;
	}
#if 0
	if(reg->start >= 0x100000000ull) {
		reg->subtype = MEMORY_AVAILABLE_PERSISTENT;
		alloc = NULL;
	}
#endif

	if(alloc && (reg->start < 0x100000000ull || 1)) {
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
	reg->off = 0;
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

#include <object.h>
uintptr_t mm_physical_alloc(size_t length, int type, bool clear)
{
	static struct spinlock nvs = SPINLOCK_INIT;
	if(type == PM_TYPE_NV) {
		/* TODO: BETTER PM MANAGEMENT */
		/* TODO: clean this up */
		// printk("ALloc NVM\n");
		foreach(e, list, &physical_regions) {
			struct memregion *reg = list_entry(e, struct memregion, entry);

			//	printk(":: %d %d\n", reg->type, reg->subtype);
			if(reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_PERSISTENT) {
				//			printk("ALLOC PER\n");
				spinlock_acquire_save(&nvs);
				static int _in = 0;
				if(!_in) {
					for(int i = 0; i < 32; i++) {
						arch_objspace_map(32ul * 1024ul * 1024ul * 1024ul + i * mm_page_size(2),
						  reg->start + i * mm_page_size(2),
						  2,
						  OBJSPACE_WRITE | OBJSPACE_READ);
					}
					_in = 1;
				}
				size_t a = length - ((reg->start + reg->off) % length);
				reg->off += a;
				size_t x = reg->off;
				if(x + length > 32ul * 1024ul * 1024ul * 1024ul)
					panic("OOPM");
				reg->off += length;
				for(int j = 0; j < length / 0x1000; j++) {
					memset((void *)0xffffff0000000000ul + x, 0, 0x1000);
				}
				spinlock_release_restore(&nvs);
				return x + reg->start;
			}
		}
		/* TODO: */
		// printk("warning - allocating volatile RAM when NVM was requested\n");
	}
	foreach(e, list, &physical_regions_alloc) {
		struct memregion *reg = list_entry(e, struct memregion, alloc_entry);

		if((reg->flags & type) == reg->flags && reg->alloc && reg->alloc->ready
		   && reg->alloc->free_memory > length) {
			uintptr_t alloc = mm_physical_region_alloc(reg, length, clear);
			//		printk("alloc: %lx\n", alloc);
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
