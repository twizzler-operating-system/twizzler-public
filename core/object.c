#include <object.h>
#include <slab.h>
#include <memory.h>
#include <lib/bitmap.h>

DECLARE_IHTABLE(objtbl, 12);
DECLARE_IHTABLE(objslots, 10);

#define NUM_TL_SLOTS (OM_ADDR_SIZE / mm_page_size(MAX_PGLEVEL) - 1)

struct slabcache sc_objs, sc_pctable, sc_objpage;

struct spinlock slotlock = SPINLOCK_INIT;

void *slot_bitmap;

static void _obj_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->pagecache = slabcache_alloc(&sc_pctable);
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
	slabcache_init(&sc_pctable, ihtable_size(4), _iht_ctor, NULL, (void *)4ul);
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

void obj_create(uint128_t id, size_t maxsz, size_t dataoff)
{
	struct object *obj = slabcache_alloc(&sc_objs);

	obj->id = id;
	obj->maxsz = maxsz;
	obj->pglevel = sz_to_pglevel(maxsz);
	obj->pglevel = 0; /* TODO */
	obj->slot = -1;
	obj->dataoff = dataoff;

	ihtable_lock(&objtbl);
	ihtable_insert(&objtbl, &obj->elem, obj->id);
	ihtable_unlock(&objtbl);
}

struct object *obj_lookup(uint128_t id)
{
	struct object *obj = ihtable_find(&objtbl, id, struct object, elem, id);
	return obj;
}

void obj_alloc_slot(struct object *obj)
{
	/* TODO: lock free? */
	spinlock_acquire(&slotlock);
	int slot = bitmap_ffr(slot_bitmap, NUM_TL_SLOTS);
	if(slot == -1)
		panic("Out of top-level slots");

	bitmap_set(slot_bitmap, slot);
	/* TODO: don't hard-code these */
	int es = slot + 4096;
	if(obj->pglevel < MAX_PGLEVEL) {
		es *= 512;
	}
	obj->slot = es;

	ihtable_insert(&objslots, &obj->slotelem, obj->slot);
	spinlock_release(&slotlock);
	printk("Assigned object " PR128FMT " slot %d (%lx)\n", PR128(obj->id), es, es * mm_page_size(obj->pglevel));
}

void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys)
{
	spinlock_acquire(&obj->lock);
	/* TODO: duplicates? */
	struct objpage *page = slabcache_alloc(&sc_objpage);
	page->idx = idx;
	page->phys = phys;
	ihtable_insert(obj->pagecache, &page->elem, page->idx);
	spinlock_release(&obj->lock);
}

struct object *obj_lookup_slot(uintptr_t oaddr)
{
	/* TODO: this is allllll bullshit */
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	tl -= 8;
	tl += 4096;
	tl *= 512;
	printk(":: %ld\n", tl);
	struct object *obj = ihtable_find(&objslots, tl, struct object, slotelem, slot);
	return obj;
}

