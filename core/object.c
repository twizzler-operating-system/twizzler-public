#include <lib/bitmap.h>
#include <lib/blake2.h>
#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <slab.h>
#include <slots.h>

/* TODO: do we need a separate objpage abstraction in addition to a page abstraction */

DECLARE_IHTABLE(objtbl, 12);
DECLARE_IHTABLE(objslots, 10);

#define NUM_TL_SLOTS (OM_ADDR_SIZE / mm_page_size(MAX_PGLEVEL) - 1)

struct slabcache sc_objs, sc_objpage;

struct spinlock objlock = SPINLOCK_INIT;
static int __objpage_compar_key(struct objpage *a, size_t n)
{
	if(a->idx > n)
		return 1;
	else if(a->idx < n)
		return -1;
	return 0;
}

static int __objpage_compar(struct objpage *a, struct objpage *b)
{
	return __objpage_compar_key(a, b->idx);
}

static void _obj_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->verlock = SPINLOCK_INIT;
	obj->tslock = SPINLOCK_INIT;
	obj->pagecache_root = RBINIT;
	obj->pagecache_level1_root = RBINIT;
	obj->tstable_root = RBINIT;
	// obj->pagecache = slabcache_alloc(&sc_pctable);
	// obj->pagecache_level1 = slabcache_alloc(&sc_pctable);
	// obj->tstable = slabcache_alloc(&sc_tstable);
}

void obj_init(struct object *obj)
{
	_obj_ctor(NULL, obj);
	obj->maxsz = mm_page_size(MAX_PGLEVEL);
	obj->pglevel = MAX_PGLEVEL;
	obj->slot = NULL;
	obj->persist = false;
	obj->kvmap = NULL;
	krc_init(&obj->refs);
	krc_init_zero(&obj->pcount);
	arch_object_init(obj);
}

static void _obj_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	assert(krc_iszero(&obj->refs));
	assert(krc_iszero(&obj->pcount));
}

static struct objpage *objpage_alloc(void)
{
	return slabcache_alloc(&sc_objpage);
}

void obj_system_init(void)
{
	/* TODO (perf): verify all ihtable sizes */
	slabcache_init(&sc_objs, sizeof(struct object), _obj_ctor, _obj_dtor, NULL);
	slabcache_init(&sc_objpage, sizeof(struct objpage), NULL, NULL, NULL);
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

void obj_pin(struct object *obj)
{
	obj->pinned = true;
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

	obj_init(obj);
	obj->id = id;
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
	for(struct rbnode *node = rb_first(&src->pagecache_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		if(pg->page) {
			struct page *np = page_alloc(pg->page->type, pg->page->level);
			/* TODO (perf): copy-on-write */
			memcpy(mm_ptov(np->addr), mm_ptov(pg->page->addr), mm_page_size(pg->page->level));
			obj_cache_page(obj, pg->idx * mm_page_size(0), np);
		}
	}
	for(struct rbnode *node = rb_first(&src->pagecache_level1_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		if(pg->page) {
			struct page *np = page_alloc(pg->page->type, pg->page->level);
			/* TODO (perf): copy-on-write */
			memcpy(mm_ptov(np->addr), mm_ptov(pg->page->addr), mm_page_size(pg->page->level));
			obj_cache_page(obj, pg->idx * mm_page_size(1), np);
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

void obj_alloc_kernel_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(obj->kslot) {
		spinlock_release_restore(&obj->lock);
		return;
	}
	struct slot *slot = slot_alloc();
	krc_get(&obj->refs);
	slot->obj = obj;   /* above get */
	obj->kslot = slot; /* move ref */
	spinlock_release_restore(&obj->lock);
}

void obj_alloc_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(obj->slot) {
		spinlock_release_restore(&obj->lock);
		return;
	}
	struct slot *slot = slot_alloc();
	krc_get(&obj->refs);
	slot->obj = obj;  /* above get */
	obj->slot = slot; /* move ref */
	spinlock_release_restore(&obj->lock);
}

void obj_cache_page(struct object *obj, size_t addr, struct page *p)
{
	if(addr & (mm_page_size(p->level) - 1))
		panic("cannot map page level %d to %lx\n", p->level, addr);
	size_t idx = addr / mm_page_size(p->level);
	struct rbroot *root = p->level ? &obj->pagecache_level1_root : &obj->pagecache_root;

	spinlock_acquire_save(&obj->lock);
	struct rbnode *node = rb_search(root, idx, struct objpage, node, __objpage_compar_key);
	struct objpage *page;
	/* TODO (major): deal with overwrites? */
	if(node == NULL) {
		page = objpage_alloc();
		page->idx = idx;
		krc_init(&page->refs);
	} else {
		page = rb_entry(node, struct objpage, node);
	}
	page->page = p;
	page->flags &= ~OBJPAGE_MAPPED;
	rb_insert(root, page, struct objpage, node, __objpage_compar);
	// arch_object_map_page(obj, page->page, page->idx);
	// page->flags |= OBJPAGE_MAPPED;
	spinlock_release_restore(&obj->lock);
}

#include <processor.h>
#include <twz/_sys.h>

struct objpage *obj_get_page(struct object *obj, size_t addr, bool alloc)
{
	size_t idx = addr / mm_page_size(1);
	spinlock_acquire_save(&obj->lock);
	struct objpage *lpage = NULL;
	struct rbnode *node =
	  rb_search(&obj->pagecache_level1_root, idx, struct objpage, node, __objpage_compar_key);
	if(node)
		lpage = rb_entry(node, struct objpage, node);
	if(lpage && lpage->page->level == 1) {
		/* found a large page */
		krc_get(&lpage->refs);
		spinlock_release_restore(&obj->lock);
		return lpage;
	}
	idx = addr / mm_page_size(0);
	struct objpage *page = NULL;
	node = rb_search(&obj->pagecache_root, idx, struct objpage, node, __objpage_compar_key);
	if(node)
		page = rb_entry(node, struct objpage, node);
	if(page == NULL) {
		if(!alloc) {
			spinlock_release_restore(&obj->lock);
			return NULL;
		}
		int level = ((addr >= mm_page_size(1)) || (obj->lowpg && 0)) ? 0 : 0; /* TODO */
		page = objpage_alloc();
		page->page = page_alloc(obj->persist ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE, level);
		//	printk("adding page %ld: %d %d\n", page->idx, page->page->level, level);
		page->idx = addr / mm_page_size(page->page->level);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_UC, PAGE_CACHE_UC);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_WB, PAGE_CACHE_WB);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_WT, PAGE_CACHE_WT);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_WC, PAGE_CACHE_WC);
		krc_init_zero(&page->refs);
		rb_insert(page->page->level ? &obj->pagecache_level1_root : &obj->pagecache_root,
		  page,
		  struct objpage,
		  node,
		  __objpage_compar);
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
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len < OBJ_MAXSIZE && len < OBJ_MAXSIZE);

	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);

	void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + start;
	memcpy(ptr, addr, len);
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len < OBJ_MAXSIZE && len < OBJ_MAXSIZE);

	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);

	void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + start;
	memcpy(addr, ptr, len);
}

void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val)
{
	off += OBJ_NULLPAGE_SIZE;
	assert(off < OBJ_MAXSIZE && off + 8 < OBJ_MAXSIZE);

	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);

	void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + off;
	*(_Atomic uint64_t *)addr = val;
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

			struct objpage *p = obj_get_page(obj, s + OBJ_NULLPAGE_SIZE, false);
			atomic_thread_fence(memory_order_seq_cst);
			blake2b_update(&S, mm_ptov(p->page->addr), rem);
			obj_put_page(p);
		}
		size_t mdbottom = OBJ_METAPAGE_SIZE + sizeof(struct fotentry) * mi.fotentries;
		size_t pos = OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + mdbottom);
		size_t thispage = mm_page_size(0);
		for(size_t s = pos; s < OBJ_MAXSIZE - OBJ_NULLPAGE_SIZE; s += thispage) {
			struct objpage *p = obj_get_page(obj, pos + OBJ_NULLPAGE_SIZE, false);

			size_t offset = pos % mm_page_size(0);
			size_t len = mm_page_size(0) - offset;
			atomic_thread_fence(memory_order_seq_cst);
			blake2b_update(&S, mm_ptov(p->page->addr + offset), len);

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
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	struct slot *slot = slot_lookup(tl);
	if(!slot) {
		return NULL;
	}
	struct object *obj = slot->obj;
	if(obj) {
		krc_get(&obj->refs);
	}
	slot_release(slot);
	return obj;
}
#include <processor.h>
#include <secctx.h>
#include <thread.h>

int obj_check_permission(struct object *obj, uint64_t flags)
{
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	struct metainfo mi;
	obj_read_data(obj, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	uint32_t dfl = mi.p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
		return 0;
	}
	return secctx_check_permissions(current_thread, arch_thread_instruction_pointer(), obj, flags);
}

static bool __objspace_fault_calculate_perms(struct object *o,
  uint32_t flags,
  uintptr_t loaddr,
  uintptr_t vaddr,
  uintptr_t ip,
  uint64_t *perms)
{
	/* optimization: just check if default permissions are enough */
	struct metainfo mi;
	obj_read_data(o, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	if(mi.magic != MI_MAGIC) {
		struct fault_object_info info = {
			.ip = ip,
			.addr = vaddr,
			.objid = o->id,
			.flags = FAULT_OBJECT_INVALID,
		};
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		obj_put(o);

		return false;
	}
	uint32_t dfl = mi.p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
	bool ok = true;
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
		*perms |= OBJSPACE_READ;
	if(dfl & MIP_DFL_WRITE)
		*perms |= OBJSPACE_WRITE;
	if(dfl & MIP_DFL_EXEC)
		*perms |= OBJSPACE_EXEC_U;
	if(!ok) {
		*perms = 0;
		if(secctx_fault_resolve(current_thread, ip, loaddr, vaddr, o->id, flags, perms) == -1) {
			obj_put(o);
			return false;
		}
	}

	bool w = (*perms & OBJSPACE_WRITE);
	if(!obj_verify_id(o, !w, w)) {
		struct fault_object_info info = {
			.ip = ip,
			.addr = vaddr,
			.objid = o->id,
			.flags = FAULT_OBJECT_INVALID,
		};
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		obj_put(o);
		return false;
	}

	if(((*perms & flags) & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))
	   != (flags & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))) {
		panic("Insufficient permissions for mapping (should be handled earlier)");
	}
	return true;
}

void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t loaddr, uintptr_t vaddr, uint32_t flags)
{
	static size_t __c = 0;
	__c++;
	size_t idx = (loaddr % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
	if(idx == 0 && !VADDR_IS_KERNEL(vaddr)) {
		struct fault_null_info info = {
			.ip = ip,
			.addr = vaddr,
		};
		thread_raise_fault(current_thread, FAULT_NULL, &info, sizeof(info));
		return;
	}

	struct object *o = obj_lookup_slot(loaddr);
	if(o == NULL) {
		panic("no object mapped to slot during object fault: vaddr=%lx, oaddr=%lx, ip=%lx",
		  vaddr,
		  loaddr,
		  ip);
	}

	uint64_t perms = 0;
	uint64_t existing_flags;
	bool do_map = false;
	// do_map = true;
	// perms = OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U;
#if 1
	if(o->kernel_obj || VADDR_IS_KERNEL(vaddr)) {
		do_map = true; // TODO: dont do this every time.
		perms = OBJSPACE_READ | OBJSPACE_WRITE;
	} else {
		if(arch_object_getmap_slot_flags(NULL, o, &existing_flags)) {
			/* we've already mapped the object. Maybe we don't need to do a permissions check. */
			if((existing_flags & flags) != flags) {
				do_map = true;
				if(!__objspace_fault_calculate_perms(o, flags, loaddr, vaddr, ip, &perms))
					return;
			}
		} else {
			do_map = true;
			if(!__objspace_fault_calculate_perms(o, flags, loaddr, vaddr, ip, &perms))
				return;
		}
	}
#endif

#if 0
	if(o->id)
		printk("OSPACE FAULT %ld: ip=%lx loaddr=%lx (idx=%lx) vaddr=%lx flags=%x :: " IDFMT "\n",
		  current_thread ? current_thread->id : -1,
		  ip,
		  loaddr,
		  idx,
		  vaddr,
		  flags,
		  IDPR(o->id));
#endif

	/* TODO: something better */
	// for(int j = 0; j < 4 && (idx < 200000 || j == 0); j++, idx++) {

	if(do_map) {
		arch_object_map_slot(NULL,
		  o,
		  VADDR_IS_KERNEL(vaddr) ? o->kslot : o->slot,
		  perms & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U));
	}
	if(o->alloc_pages) {
		struct objpage p;
		p.page = page_alloc(PAGE_TYPE_VOLATILE, 0); /* TODO: refcount, largepage */
		p.idx = (loaddr % OBJ_MAXSIZE) / mm_page_size(p.page->level);
		p.page->flags = PAGE_CACHE_WB;
		arch_object_map_page(o, &p);
	} else {
		struct objpage *p = obj_get_page(o, loaddr % OBJ_MAXSIZE, true);
		// printk("P: %p %lx\n", p->page, p->page->addr);
		if(!(p->flags & OBJPAGE_MAPPED)) {
			arch_object_map_page(o, p);
			p->flags |= OBJPAGE_MAPPED;
		}
	}
	// printk("Mapped successfully\n");
	//	do_map = false;
	//}
	/* TODO: put page? */

	obj_put(o);
}
