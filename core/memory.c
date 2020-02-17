#include <debug.h>
#include <lib/iter.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <slots.h>
#include <thread.h>
static DECLARE_LIST(physical_regions);

static DECLARE_LIST(allocators);
static struct mem_allocator _initial_allocator;

#define MAX_ALLOCATORS 16
static struct spinlock allocator_lock = SPINLOCK_INIT;

static const char *memory_type_strings[] = {
	[MEMORY_AVAILABLE] = "System RAM",
	[MEMORY_RESERVED] = "Reserved",
	[MEMORY_CODE] = "Firmware Code",
	[MEMORY_BAD] = "Bad Memory",
	[MEMORY_RECLAIMABLE] = "Reclaimable System Memory",
	[MEMORY_UNKNOWN] = "Unknown Memory",
	[MEMORY_KERNEL_IMAGE] = "Kernel Image",
};

static const char *memory_subtype_strings[] = {
	[MEMORY_SUBTYPE_NONE] = "",
	[MEMORY_AVAILABLE_VOLATILE] = "(volatile)",
	[MEMORY_AVAILABLE_PERSISTENT] = "(persistent)",
};

void mm_register_region(struct memregion *reg)
{
	printk("[mm] registering memory region %lx -> %lx %s %s\n",
	  reg->start,
	  reg->start + reg->length - 1,
	  memory_type_strings[reg->type],
	  memory_subtype_strings[reg->subtype]);

	list_insert(&physical_regions, &reg->entry);

	if(mm_ready) {
		/* registration of a region triggers this if the memory manager is fully ready. If it's not,
		 * we need to hold off until we can create the page stacks. */
		if(reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_VOLATILE) {
			page_init(reg);
			reg->ready = true;
		}
	}
}

uintptr_t mm_vtoo(void *addr)
{
	uintptr_t phys;
	int level;
	if(!arch_vm_getmap(NULL, (uintptr_t)addr, &phys, &level, NULL))
		panic("mm_vtoo with unmapped addr");
	return phys + (uintptr_t)addr % mm_page_size(level);
}

uintptr_t mm_otop(uintptr_t oaddr)
{
	struct object *o = obj_lookup_slot(oaddr);
	if(!o) {
		panic("mm_otop no object");
	}
	assert(o->flags & OF_KERNEL);
	int level;
	uintptr_t phys;
	/* TODO: this might happen if we allocate memory, don't clear it, and then try to compute the
	 * physical address before touching it. */
	if(!arch_object_getmap(o, oaddr % OBJ_MAXSIZE, &phys, &level, NULL))
		panic("mm_otop no such mapping");
	obj_put(o);
	// printk(":: %lx %lx %lx\n", oaddr, phys, oaddr % mm_page_size(level));

	return phys + oaddr % mm_page_size(level);
}

uintptr_t mm_vtop(void *addr)
{
	uintptr_t v = (uintptr_t)addr;
	if(v >= (uintptr_t)SLOT_TO_VADDR(KVSLOT_BOOTSTRAP)
	   && v <= (uintptr_t)SLOT_TO_VADDR(KVSLOT_BOOTSTRAP) + (OBJ_MAXSIZE - 1)) {
		//	printk("vtop: %p -> %lx\n", addr, (uintptr_t)addr - PHYSICAL_MAP_START);
		return (uintptr_t)addr - PHYSICAL_MAP_START;
	}
	uintptr_t oaddr = mm_vtoo(addr);
	uintptr_t paddr = mm_otop(oaddr);
	// printk("vtop: %p -> %lx -> %lx\n", addr, oaddr, paddr);
	return paddr;
}

void *mm_ptov(uintptr_t addr)
{
	uintptr_t p = (uintptr_t)addr;
	if(p >= SLOT_TO_OADDR(0) && p <= SLOT_TO_OADDR(0) + (OBJ_MAXSIZE - 1)) {
		return (void *)((uintptr_t)addr + PHYSICAL_MAP_START);
	}
	panic("UNIMP");
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
	reg->type = type;
	reg->subtype = st;
	reg->off = 0;
}

void mm_init(void)
{
	arch_mm_init();
}

size_t mm_early_count = 0;
size_t mm_late_count = 0;
extern size_t mm_page_count;
extern size_t mm_page_alloc_count;
extern size_t mm_page_bootstrap_count;
extern size_t mm_page_alloced;
bool mm_ready = false;

void mm_print_stats(void)
{
	printk("early allocation: %ld KB\n", mm_early_count / 1024);
	printk("late  allocation: %ld KB\n", mm_late_count / 1024);
	printk("page  allocation: %ld KB\n", mm_page_alloc_count / 1024);
	page_print_stats();
}

void mm_init_phase_2(void)
{
	obj_system_init();
	slot_init_bootstrap(KOSLOT_INIT_ALLOC, KVSLOT_ALLOC_START);
	_initial_allocator.start = SLOT_TO_OADDR(KOSLOT_INIT_ALLOC);
	_initial_allocator.vstart = SLOT_TO_VADDR(KVSLOT_ALLOC_START);
	_initial_allocator.length = OBJ_MAXSIZE;
	pmm_buddy_init(&_initial_allocator);
	list_insert(&allocators, &_initial_allocator.entry);

	// allocs[0].start = SLOT_TO_OADDR(KOSLOT_INIT_ALLOC) + 0x8000;
	// allocs[0].vstart = SLOT_TO_VADDR(KVSLOT_ALLOC_START) + 0x8000;
	// allocs[0].length = OBJ_MAXSIZE - 0x8000;
	// pmm_buddy_init(&allocs[0]);
	// list_insert(&allocators, &allocs[0].entry);

	foreach(e, list, &physical_regions) {
		struct memregion *reg = list_entry(e, struct memregion, entry);
		if(reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_VOLATILE) {
			page_init(reg);
			reg->ready = true;
		}
	}
	slots_init();

	mm_ready = true;
	mm_print_stats();
}

uintptr_t mm_physical_early_alloc(void)
{
	if(!list_empty(&allocators)) {
		panic("tried to early-alloc after mm_phase_2");
	}
	foreach(e, list, &physical_regions) {
		struct memregion *reg = list_entry(e, struct memregion, entry);

		/* reg->ready indicates that the memory is ready for allocation -- early allocator won't
		 * work.
		 * */
		if(reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_VOLATILE
		   && reg->start < OBJ_MAXSIZE && reg->length > 0 && reg->start > 0 && !reg->ready) {
			uintptr_t alloc = reg->start;
			reg->start += mm_page_size(0);
			reg->length -= mm_page_size(0);
			assert(alloc);
			mm_early_count += mm_page_size(0);
			return alloc;
		}
	}
	panic("out of early-alloc memory");
}

void *mm_memory_alloc(size_t length, int type, bool clear)
{
	length = align_up(length, mm_page_size(0));
	static struct spinlock nvs = SPINLOCK_INIT;
	if(type == PM_TYPE_NV) {
		/* TODO: BETTER PM MANAGEMENT */
		/* TODO: clean this up */
		// printk("ALloc NVM\n");
		panic("fix nvm alloc");
		foreach(e, list, &physical_regions) {
			struct memregion *reg = list_entry(e, struct memregion, entry);

			//	printk(":: %d %d\n", reg->type, reg->subtype);
			if(reg->type == MEMORY_AVAILABLE && reg->subtype == MEMORY_AVAILABLE_PERSISTENT) {
				//			printk("ALLOC PER\n");
				spinlock_acquire_save(&nvs);
				static int _in = 0;
				if(!_in) {
					for(int i = 0; i < 32; i++) {
						// arch_objspace_map(32ul * 1024ul * 1024ul * 1024ul + i * mm_page_size(2),
						//  reg->start + i * mm_page_size(2),
						//  2,
						// OBJSPACE_WRITE | OBJSPACE_READ);
					}
					_in = 1;
				}
				size_t a = length - ((reg->start + reg->off) % length);
				reg->off += a;
				size_t x = reg->off;
				if(x + length > 32ul * 1024ul * 1024ul * 1024ul)
					panic("OOPM");
				reg->off += length;
				for(unsigned int j = 0; j < length / 0x1000; j++) {
					memset((void *)0xffffff0000000000ul + x, 0, 0x1000);
				}
				spinlock_release_restore(&nvs);
				if(reg->off % (1024 * 1024 * 1024) == 0)
					printk("GB ALLOCATED\n");
				return (void *)(x + reg->start);
			}
		}
		/* TODO: */
		// printk("warning - allocating volatile RAM when NVM was requested\n");
	}
	if(list_empty(&allocators)) {
		/* still in bootstrap mode */
		if(length != mm_page_size(0))
			panic("tried to allocate non-page-size memory in bootstrap mode: %lx", length);
		void *p = mm_virtual_early_alloc();
		// printk("early alloc: %p\n", p);
		return p;
	}
	spinlock_acquire_save(&allocator_lock);
	foreach(e, list, &allocators) {
		struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);
		// printk(":: alloc %lx: %ld %ld\n", length, alloc->free_memory, alloc->available_memory);
		if(alloc->available_memory >= length) {
			void *p = (void *)alloc->marker;
			alloc->available_memory -= length;
			alloc->marker += length;
			mm_late_count += length;
			spinlock_release_restore(&allocator_lock);
			if(clear) {
				memset(p, 0, length);
			}
			// printk("aalloc: %p\n", p);
			return (void *)p;
		}
		if(alloc->free_memory > length) {
			uintptr_t x = pmm_buddy_allocate(alloc, length);
			if(x != 0) {
				mm_late_count += length;
				spinlock_release_restore(&allocator_lock);
				if(clear)
					memset((void *)x, 0, length);
				// printk("alloc: %p\n", x);
				return (void *)x;
			}
		}
	}

	//	if(allocator_ctr >= MAX_ALLOCATORS)
	panic("OOM"); /* TODO: reclaim, etc */

	//	struct mem_allocator *alloc = allocators[allocator_ctr++];

	spinlock_release_restore(&allocator_lock);
	return 0;
}

void mm_memory_dealloc(void *_addr)
{
	uintptr_t addr = (uintptr_t)_addr;
	foreach(e, list, &allocators) {
		struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);
		if(addr >= (uintptr_t)alloc->vstart && addr < (uintptr_t)alloc->vstart + alloc->length) {
			pmm_buddy_deallocate(alloc, addr);
			return;
		}
	}
	panic("invalid free");
}

void kernel_fault_entry(uintptr_t ip, uintptr_t addr, int flags)
{
	if(addr < KERNEL_VIRTUAL_BASE) {
		vm_context_fault(ip, addr, flags);
	} else {
		panic("kernel page fault: %lx, %x at ip=%lx", addr, flags, ip);
	}
}
