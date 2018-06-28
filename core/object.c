#include <object.h>
#include <slab.h>
#include <memory.h>
#include <lib/bitmap.h>

DECLARE_IHTABLE(objtbl, 12);
DECLARE_IHTABLE(objslots, 10);

#define NUM_TL_SLOTS (OM_ADDR_SIZE / mm_page_size(MAX_PGLEVEL) - 1)

struct slabcache sc_objs, sc_pctable, sc_tstable, sc_objpage;

struct spinlock slotlock = SPINLOCK_INIT;

void *slot_bitmap;

static void _obj_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->pagecache = slabcache_alloc(&sc_pctable);
	obj->tstable = slabcache_alloc(&sc_tstable);
}

static void _obj_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	slabcache_free(obj->pagecache);
}

__initializer
static void _init_objs(void)
{
	/* TODO (perf): verify all ihtable sizes */
	slabcache_init(&sc_pctable, ihtable_size(4), _iht_ctor, NULL, (void *)4ul);
	slabcache_init(&sc_tstable, ihtable_size(4), _iht_ctor, NULL, (void *)4ul);
	slabcache_init(&sc_objs, sizeof(struct object), _obj_ctor, _obj_dtor, NULL);
	slabcache_init(&sc_objpage, sizeof(struct objpage), NULL, NULL, NULL);
	slot_bitmap = (void *)mm_virtual_alloc(NUM_TL_SLOTS / 8, PM_TYPE_DRAM, true);
	printk("Allocated %ld KB for object slots\n", NUM_TL_SLOTS / (8 * 1024));
}

static int sz_to_pglevel(size_t sz)
{
	for(int i=0;i<MAX_PGLEVEL;i++) {
		if(sz < mm_page_size(i))
			return i;
	}
	return MAX_PGLEVEL;
}

static void obj_kso_init(struct object *obj, enum kso_type ksot)
{
	obj->kso_type = ksot;
}

struct object *obj_create(uint128_t id, enum kso_type ksot)
{
	struct object *obj = slabcache_alloc(&sc_objs);

	obj->id = id;
	obj->maxsz = mm_page_size(MAX_PGLEVEL);
	obj->pglevel = MAX_PGLEVEL;
	obj->slot = -1;

	obj_kso_init(obj, ksot);

	ihtable_lock(&objtbl);
	ihtable_insert(&objtbl, &obj->elem, obj->id);
	ihtable_unlock(&objtbl);
	return obj;
}

struct object *obj_lookup(uint128_t id)
{
	struct object *obj = ihtable_find(&objtbl, id, struct object, elem, id);
	return obj;
}

void obj_alloc_slot(struct object *obj)
{
	/* TODO: lock free? */
	bool fl = spinlock_acquire(&slotlock);
	int slot = bitmap_ffr(slot_bitmap, NUM_TL_SLOTS);
	if(slot == -1)
		panic("Out of top-level slots");

	bitmap_set(slot_bitmap, slot);
	/* TODO: don't hard-code these */
	int es = slot + 16;
	if(obj->pglevel < MAX_PGLEVEL) {
		es *= 512;
	}
	obj->slot = es;

	ihtable_insert(&objslots, &obj->slotelem, obj->slot);
	spinlock_release(&slotlock, fl);
	printk("Assigned object " PR128FMT " slot %d (%lx)\n", PR128(obj->id), es, es * mm_page_size(obj->pglevel));
}

void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys)
{
	bool fl = spinlock_acquire(&obj->lock);
	/* TODO: duplicates? */
	struct objpage *page = slabcache_alloc(&sc_objpage);
	page->idx = idx;
	if(phys == 0) {
		phys = mm_physical_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
	}
	page->phys = phys;
	ihtable_insert(obj->pagecache, &page->elem, page->idx);
	spinlock_release(&obj->lock, fl);
}

struct objpage *obj_get_page(struct object *obj, size_t idx)
{
	spinlock_acquire_save(&obj->lock);
	struct objpage *page = ihtable_find(obj->pagecache, idx, struct objpage, elem, idx);
	if(page == NULL) {
		page = slabcache_alloc(&sc_objpage);
		page->idx = idx;
		page->phys = mm_physical_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
		ihtable_insert(obj->pagecache, &page->elem, page->idx);
	}
	spinlock_release_restore(&obj->lock);
	return page;
}

void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	if(start / mm_page_size(0) != (start + len) / mm_page_size(0)) {
		panic("NI - cross-page KSO read");
	}
	struct objpage *p = obj_get_page(obj, start / mm_page_size(0));
	atomic_thread_fence(memory_order_seq_cst);
	memcpy(ptr, mm_ptov(p->phys + (start % mm_page_size(0))), len);
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	if(start / mm_page_size(0) != (start + len) / mm_page_size(0)) {
		panic("NI - cross-page KSO read");
	}
	struct objpage *p = obj_get_page(obj, start / mm_page_size(0));
	memcpy(mm_ptov(p->phys + (start % mm_page_size(0))), ptr, len);
	atomic_thread_fence(memory_order_seq_cst);
}

struct object *obj_lookup_slot(uintptr_t oaddr)
{
	/* TODO: this is allllll bullshit */
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	//tl -= 8;
	//tl += 4096;
	//tl *= 512;
	struct object *obj = ihtable_find(&objslots, tl, struct object, slotelem, slot);
	return obj;
}

bool arch_objspace_map(uintptr_t v, uintptr_t p, int level, uint64_t flags);
void kernel_objspace_fault_entry(uintptr_t addr, uint32_t flags)
{
	size_t slot = addr / mm_page_size(MAX_PGLEVEL);
	size_t idx = (addr % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
	if(idx == 0) {
		panic("NULL PAGE");
	}
	idx -= 1;

	struct object *o = obj_lookup_slot(addr);
	if(o == NULL) {
		panic("NO OBJ");
	}

	struct objpage *p = obj_get_page(o, idx);

	arch_objspace_map(addr & ~(mm_page_size(0) - 1), p->phys, 0, OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U);
}

