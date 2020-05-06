#include <debug.h>
#include <lib/iter.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <pmap.h>
#include <slots.h>
#include <thread.h>
#include <tmpmap.h>
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
	struct slot *slot;
	struct object *o = obj_lookup_slot(oaddr, &slot);
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
	slot_release(slot);
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

void *mm_ptov_try(uintptr_t addr)
{
	uintptr_t p = (uintptr_t)addr;
	if(p >= SLOT_TO_OADDR(0) && p <= SLOT_TO_OADDR(0) + (OBJ_MAXSIZE - 1)) {
		return (void *)((uintptr_t)addr + PHYSICAL_MAP_START);
	}
	return NULL;
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
_Atomic size_t mm_kernel_alloced_total = 0;
_Atomic size_t mm_kernel_alloced_current = 0;
extern size_t mm_page_count;
extern size_t mm_page_alloc_count;
extern size_t mm_page_bootstrap_count;
extern size_t mm_page_alloced;
bool mm_ready = false;

#include <twz/driver/memory.h>

struct memory_stats mm_stats = { 0 };
struct page_stats mm_page_stats[MAX_PGLEVEL + 1];

void mm_print_stats(void)
{
	printk("early allocation: %ld KB\n", mm_early_count / 1024);
	printk("late  allocation: %ld KB\n", mm_late_count / 1024);
	printk("page  allocation: %ld KB\n", mm_page_alloc_count / 1024);
	printk("total allocation: %ld KB\n", mm_kernel_alloced_total);
	printk("cur   allocation: %ld KB\n", mm_kernel_alloced_current);
	foreach(e, list, &allocators) {
		struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);
		printk("allocator: avail = %lx; free = %lx\n", alloc->available_memory, alloc->free_memory);
	}
	page_print_stats();
}

#include <device.h>
#include <init.h>
#include <object.h>
#include <twz/driver/system.h>
static struct object *mem_object = NULL;
static struct memory_stats_header *msh = NULL;
static void __init_mem_object(void *_a __unused)
{
	struct object *so = get_system_object();
	struct object *d = device_register(DEVICE_BT_SYSTEM, 1ul << 24);
	char name[128];
	snprintf(name, 128, "MEMORY STATS");
	kso_setname(d, name);

	struct bus_repr *brepr = bus_get_repr(so);
	kso_attach(so, d, brepr->max_children);
	device_release_headers(so);
	mem_object = d; /* krc: move */
	msh = device_get_devspecific(mem_object);
}
POST_INIT(__init_mem_object, NULL);

void mm_update_stats(void)
{
	if(msh) {
		pmap_collect_stats(&mm_stats);
		tmpmap_collect_stats(&mm_stats);
		mm_stats.pages_early_used = mm_early_count;

		size_t ma_free = 0, ma_total = 0, ma_unfreed = 0, ma_used = 0;
		size_t count = 0;
		spinlock_acquire_save(&allocator_lock);
		foreach(e, list, &allocators) {
			struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);

			ma_total += alloc->length;
			ma_used += alloc->length - (alloc->free_memory + alloc->available_memory);
			ma_unfreed += alloc->available_memory;
			ma_free += alloc->free_memory;
			count++;
		}
		spinlock_release_restore(&allocator_lock);

		mm_stats.memalloc_nr_objects = count;
		mm_stats.memalloc_total = ma_total;
		mm_stats.memalloc_used = ma_used;
		mm_stats.memalloc_unfreed = ma_unfreed;
		mm_stats.memalloc_free = ma_free;

		msh->stats = mm_stats;
		int i = 0;
		while(page_build_stats(&msh->page_stats[i], i) != -1)
			i++;
		msh->nr_page_groups = i;
	}
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
	// mm_print_stats();
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

#include <processor.h>
void *__mm_memory_alloc(size_t length, int type, bool clear, const char *file, int linenr)
{
	length = align_up(length, mm_page_size(0));
	mm_kernel_alloced_total += length;
	mm_kernel_alloced_current += length;
	// printk("ALLOC %lx: %s %d\n", length, file, linenr);
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
	bool old_cf = page_set_crit_flag();
	foreach(e, list, &allocators) {
		struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);
#if 0
		printk(":: alloc %lx: %ld %ld: %s:%d\n",
		  length,
		  alloc->free_memory,
		  alloc->available_memory,
		  file,
		  linenr);
#endif
		// debug_print_backtrace();
		/* TODO: the buddy allocator doesn't seem to like non-0x1000 sized allocs.
		 * But: do we even need to allocate larger bits of memory than that most of the time? */
		if(alloc->free_memory > length && length == 0x1000) {
			uintptr_t x = pmm_buddy_allocate(alloc, length);
			if(x != 0) {
				mm_late_count += length;
				page_reset_crit_flag(old_cf);
				spinlock_release_restore(&allocator_lock);
				if(clear)
					memset((void *)x, 0, length);
				// printk("alloc: %p\n", x);
				return (void *)x;
			}
		}

		if(alloc->available_memory >= length) {
			void *p = (void *)alloc->marker;
			alloc->available_memory -= length;
			alloc->marker += length;
			mm_late_count += length;
			page_reset_crit_flag(old_cf);
			spinlock_release_restore(&allocator_lock);
			if(clear) {
				memset(p, 0, length);
			}
			// printk("aalloc: %p\n", p);
			return (void *)p;
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
	mm_kernel_alloced_current -= mm_page_size(0); // TODO: this isn't accurate
	spinlock_acquire_save(&allocator_lock);
	foreach(e, list, &allocators) {
		struct mem_allocator *alloc = list_entry(e, struct mem_allocator, entry);
		if(addr >= (uintptr_t)alloc->vstart && addr < (uintptr_t)alloc->vstart + alloc->length) {
			pmm_buddy_deallocate(alloc, addr);
			spinlock_release_restore(&allocator_lock);
			return;
		}
	}
	spinlock_release_restore(&allocator_lock);
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
