#include <lib/bitmap.h>
#include <lib/blake2.h>
#include <memory.h>
#include <object.h>
#include <slab.h>

DECLARE_IHTABLE(objtbl, 12);
DECLARE_IHTABLE(objslots, 10);

#define NUM_TL_SLOTS (OM_ADDR_SIZE / mm_page_size(MAX_PGLEVEL) - 1)

struct slabcache sc_objs, sc_pctable, sc_tstable, sc_objpage;

struct spinlock slotlock = SPINLOCK_INIT;
struct spinlock objlock = SPINLOCK_INIT;

void *slot_bitmap;

static void _obj_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->verlock = SPINLOCK_INIT;
	obj->tslock = SPINLOCK_INIT;
	obj->pagecache = slabcache_alloc(&sc_pctable);
	obj->tstable = slabcache_alloc(&sc_tstable);
}

static void _obj_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	assert(krc_iszero(&obj->refs));
	assert(krc_iszero(&obj->pcount));
	slabcache_free(obj->pagecache);
}

__initializer static void _init_objs(void)
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
	for(int i = 0; i < MAX_PGLEVEL; i++) {
		if(sz < mm_page_size(i))
			return i;
	}
	return MAX_PGLEVEL;
}

static struct kso_calls *_kso_calls[KSO_MAX];

void kso_register(int t, struct kso_calls *c)
{
	_kso_calls[t] = c;
}

struct kso_calls *kso_lookup_calls(int t)
{
	return _kso_calls[t];
}

void kso_detach_event(struct thread *thr, bool entry, int sysc)
{
	for(size_t i = 0; i < KSO_MAX; i++) {
		if(_kso_calls[i] && _kso_calls[i]->detach_event) {
			_kso_calls[i]->detach_event(thr, entry, sysc);
		}
	}
}

void obj_kso_init(struct object *obj, enum kso_type ksot)
{
	obj->kso_type = ksot;
	obj->kso_calls = _kso_calls[ksot];
	if(obj->kso_calls && obj->kso_calls->ctor) {
		obj->kso_calls->ctor(obj);
	}
}

static inline struct object *__obj_alloc(enum kso_type ksot, objid_t id)
{
	struct object *obj = slabcache_alloc(&sc_objs);

	obj->id = id;
	obj->maxsz = mm_page_size(MAX_PGLEVEL);
	obj->pglevel = MAX_PGLEVEL;
	obj->slot = -1;
	krc_init(&obj->refs);
	krc_init_zero(&obj->pcount);
	obj_kso_init(obj, ksot);

	return obj;
}

struct object *obj_create(uint128_t id, enum kso_type ksot)
{
	struct object *obj = __obj_alloc(ksot, id);
	/* TODO (major): check for duplicates */
	if(id) {
		spinlock_acquire_save(&objlock);
		ihtable_insert(&objtbl, &obj->elem, obj->id);
		spinlock_release_restore(&objlock);
	}
	return obj;
}

void obj_assign_id(struct object *obj, objid_t id)
{
	spinlock_acquire_save(&objlock);
	if(obj->id) {
		panic("tried to reassign object ID");
	}
	obj->id = id;
	ihtable_insert(&objtbl, &obj->elem, obj->id);
	spinlock_release_restore(&objlock);
}

struct object *obj_create_clone(uint128_t id, objid_t srcid, enum kso_type ksot)
{
	struct object *src = obj_lookup(srcid);
	if(src == NULL) {
		return NULL;
	}
	struct object *obj = __obj_alloc(ksot, id);

	spinlock_acquire_save(&src->lock);
	for(size_t b = ihtable_iter_start(src->pagecache); b != ihtable_iter_end(src->pagecache);
	    b = ihtable_iter_next(b)) {
		for(struct ihelem *e = ihtable_bucket_iter_start(src->pagecache, b);
		    e != ihtable_bucket_iter_end(src->pagecache);
		    e = ihtable_bucket_iter_next(e)) {
			struct objpage *pg = container_of(e, struct objpage, elem);
			if(pg->phys) {
				void *np = (void *)mm_virtual_alloc(0x1000, PM_TYPE_DRAM, false);
				/* TODO (perf): copy-on-write */
				memcpy(np, mm_ptov(pg->phys), 0x1000);
				obj_cache_page(obj, pg->idx, mm_vtop(np));
			}
		}
	}
	spinlock_release_restore(&src->lock);

	if(id) {
		spinlock_acquire_save(&objlock);
		ihtable_insert(&objtbl, &obj->elem, obj->id);
		spinlock_release_restore(&objlock);
	}
	return obj;
}

struct object *obj_lookup(uint128_t id)
{
	spinlock_acquire_save(&objlock);
	struct object *obj = ihtable_find(&objtbl, id, struct object, elem, id);
	if(obj) {
		krc_get(&obj->refs);
	}
	spinlock_release_restore(&objlock);
	return obj;
}

void obj_alloc_slot(struct object *obj)
{
	/* TODO: lock free? */
	krc_get(&obj->refs);
	spinlock_acquire_save(&slotlock);
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
	spinlock_release_restore(&slotlock);
	// printk("Assigned object " PR128FMT " slot %d (%lx)\n",
	//		PR128(obj->id), es, es * mm_page_size(obj->pglevel));
}

void obj_cache_page(struct object *obj, size_t idx, uintptr_t phys)
{
	spinlock_acquire_save(&obj->lock);
	struct objpage *page = ihtable_find(obj->pagecache, idx, struct objpage, elem, idx);
	/* TODO (major): deal with overwrites? */
	if(page == NULL) {
		page = slabcache_alloc(&sc_objpage);
		page->idx = idx;
		krc_init(&page->refs);
		if(phys == 0) {
			phys = mm_physical_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
		}
	}
	page->phys = phys;
	ihtable_insert(obj->pagecache, &page->elem, page->idx);
	spinlock_release_restore(&obj->lock);
}

struct objpage *obj_get_page(struct object *obj, size_t idx)
{
	spinlock_acquire_save(&obj->lock);
	struct objpage *page = ihtable_find(obj->pagecache, idx, struct objpage, elem, idx);
	if(page == NULL) {
		page = slabcache_alloc(&sc_objpage);
		page->idx = idx;
		page->phys = mm_physical_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
		krc_init_zero(&page->refs);
		ihtable_insert(obj->pagecache, &page->elem, page->idx);
	}
	krc_get(&page->refs);
	spinlock_release_restore(&obj->lock);
	return page;
}

static void _objpage_release(void *page)
{
	(void)page; /* TODO (major): implement */
}

static void _obj_release(void *obj)
{
	(void)obj; /* TODO (major): implement */
}

void obj_put_page(struct objpage *p)
{
	krc_put_call(p, refs, _objpage_release);
}

void obj_put(struct object *o)
{
	krc_put_call(o, refs, _obj_release);
}

/* TODO (major): these can probably fail */
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	if(len >= mm_page_size(0)) {
		panic("NI - big KSO write");
	}
	if(start / mm_page_size(0) != (start + len) / mm_page_size(0)) {
		panic("NI - cross-page KSO read");
	}
	struct objpage *p = obj_get_page(obj, start / mm_page_size(0));
	atomic_thread_fence(memory_order_seq_cst);
	memcpy(ptr, mm_ptov(p->phys + (start % mm_page_size(0))), len);
	obj_put_page(p);
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	if(len >= mm_page_size(0)) {
		panic("NI - big KSO write");
	}
	if(start / mm_page_size(0) != (start + len) / mm_page_size(0)) {
		panic("NI - cross-page KSO read");
	}
	struct objpage *p = obj_get_page(obj, start / mm_page_size(0));
	memcpy(mm_ptov(p->phys + (start % mm_page_size(0))), ptr, len);
	atomic_thread_fence(memory_order_seq_cst);
	obj_put_page(p);
}

/* TODO (major): with these, and the above, support obj_get_page returning "no page
 * associated with this location, because there's no data here" */
objid_t obj_compute_id(struct object *obj)
{
	struct metainfo mi;
	obj_read_data(obj, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);

	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &mi.nonce, sizeof(mi.nonce));
	blake2b_update(&S, &mi.p_flags, sizeof(mi.p_flags));
	blake2b_update(&S, &mi.kuid, sizeof(mi.kuid));
	if(mi.p_flags & MIP_HASHDATA) {
		for(size_t s = 0; s < mi.sz; s += mm_page_size(0)) {
			size_t rem = mm_page_size(0);
			if(s + mm_page_size(0) > mi.sz) {
				rem = mi.sz - s;
			}
			assert(rem <= mm_page_size(0));

			struct objpage *p = obj_get_page(obj, s / mm_page_size(0));
			atomic_thread_fence(memory_order_seq_cst);
			blake2b_update(&S, mm_ptov(p->phys), rem);
			obj_put_page(p);
		}
		for(size_t s = 0; s < mi.mdbottom; s += mm_page_size(0)) {
			size_t rem = mm_page_size(0);
			if(s + mm_page_size(0) > mi.mdbottom) {
				rem = mi.mdbottom - s;
			}
			assert(rem <= mm_page_size(0));

			struct objpage *p = obj_get_page(
			  obj, (OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + (mi.mdbottom - s))) / mm_page_size(0));
			atomic_thread_fence(memory_order_seq_cst);
			blake2b_update(&S, mm_ptov(p->phys), rem);
			obj_put_page(p);
		}
	}

	unsigned char tmp[32];
	blake2b_final(&S, tmp, 32);
	_Alignas(16) unsigned char out[16];
	for(int i = 0; i < 16; i++) {
		out[i] = tmp[i] ^ tmp[i + 16];
	}
	return *(objid_t *)out;
}

bool obj_verify_id(struct object *obj, bool cache_result, bool uncache)
{
	bool result = false;
	spinlock_acquire_save(&obj->verlock);

	if(obj->idversafe) {
		result = obj->idvercache;
	} else {
		objid_t c = obj_compute_id(obj);
		result = c == obj->id;
		obj->idvercache = result && cache_result;
	}
	obj->idversafe = !uncache;
	spinlock_release_restore(&obj->verlock);
	return result;
}

struct object *obj_lookup_slot(uintptr_t oaddr)
{
	/* TODO: this is allllll bullshit */
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	// tl -= 8;
	// tl += 4096;
	// tl *= 512;
	spinlock_acquire_save(&slotlock);
	struct object *obj = ihtable_find(&objslots, tl, struct object, slotelem, slot);
	if(obj) {
		krc_get(&obj->refs);
	}
	spinlock_release_restore(&slotlock);
	return obj;
}

bool arch_objspace_map(uintptr_t v, uintptr_t p, int level, uint64_t flags);
#include <processor.h>
#include <secctx.h>
#include <thread.h>
void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t loaddr, uintptr_t vaddr, uint32_t flags)
{
	size_t slot = loaddr / mm_page_size(MAX_PGLEVEL);
	size_t idx = (loaddr % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
	if(idx == 0) {
		struct fault_null_info info = {
			.ip = ip,
			.addr = vaddr,
		};
		thread_raise_fault(current_thread, FAULT_NULL, &info, sizeof(info));
		return;
	}
	idx -= 1;

	struct object *o = obj_lookup_slot(loaddr);
	if(o == NULL) {
		panic("NO OBJ");
	}

	/*
	printk("OSPACE FAULT: ip=%lx loaddr=%lx vaddr=%lx flags=%x :: " IDFMT "\n",
	  ip,
	  loaddr,
	  vaddr,
	  flags,
	  IDPR(o->id));*/
	/* optimization: just check if default permissions are enough */
	struct metainfo mi;
	obj_read_data(o, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	uint32_t dfl = mi.p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
	bool ok = true;
	uint64_t perms = 0;
	if(flags & OBJSPACE_FAULT_READ) {
		ok = ok && (dfl & MIP_DFL_READ);
	}
	if(flags & OBJSPACE_FAULT_WRITE) {
		ok = ok && (dfl & MIP_DFL_WRITE);
	}
	if(flags & OBJSPACE_FAULT_EXEC) {
		ok = ok && (dfl & MIP_DFL_EXEC);
	}
	if(dfl & MIP_DFL_READ)
		perms |= OBJSPACE_READ;
	if(dfl & MIP_DFL_WRITE)
		perms |= OBJSPACE_WRITE;
	if(dfl & MIP_DFL_EXEC)
		perms |= OBJSPACE_EXEC_U;
	if(!ok) {
		perms = 0;
		if(secctx_fault_resolve(current_thread, ip, loaddr, vaddr, o->id, flags, &perms) == -1) {
			obj_put(o);
			return;
		}
	}

	bool w = (perms & OBJSPACE_WRITE);
	if(!obj_verify_id(o, !w, w)) {
		struct fault_object_info info = {
			.ip = ip,
			.addr = vaddr,
			.objid = o->id,
			.flags = FAULT_OBJECT_INVALID,
		};
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		obj_put(o);
		return;
	}

	if(((perms & flags) & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))
	   != (flags & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))) {
		panic("Insufficient permissions for mapping (should be handled earlier)");
	}
	/*
	printk("--> %lx %lx %d (%x)\n",
	  loaddr & ~(mm_page_size(0) - 1),
	  perms & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U),
	  ok,
	  mi.p_flags);*/

	struct objpage *p = obj_get_page(o, idx);
	obj_put(o);
	bool r = arch_objspace_map(loaddr & ~(mm_page_size(0) - 1),
	  p->phys,
	  0,
	  (perms & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U)) | OBJSPACE_SET_FLAGS);
	if(!r) {
		uintptr_t pa;
		uint64_t fl;
		int level;
		arch_objspace_getmap(loaddr & ~(mm_page_size(0) - 1), &pa, &level, &fl);
		panic("already mapped %lx %d %lx\n", pa, level, fl);
	}
	/* TODO (major): deal with mapcounting */
}
