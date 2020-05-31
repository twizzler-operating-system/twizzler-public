#include <lib/bitmap.h>
#include <lib/blake2.h>
#include <lib/iter.h>
#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <slab.h>
#include <slots.h>
#include <tmpmap.h>

static _Atomic size_t obj_count = 0;
static struct rbroot obj_tree = RBINIT;

static struct slabcache sc_objs, sc_objpage;

static DECLARE_SLABCACHE(sc_objtie, sizeof(struct object_tie), NULL, NULL, NULL);

static struct spinlock objlock = SPINLOCK_INIT;

void obj_print_stats(void)
{
	printk("KNOWN OBJECTS: %ld\n", obj_count);
}

static int __objtie_compar_key(struct object_tie *a, objid_t n)
{
	if(a->child->id > n)
		return 1;
	else if(a->child->id < n)
		return -1;
	return 0;
}

static int __objtie_compar(struct object_tie *a, struct object_tie *b)
{
	return __objtie_compar_key(a, b->child->id);
}

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

static int __obj_compar_key(struct object *a, objid_t b)
{
	if(a->id > b)
		return 1;
	else if(a->id < b)
		return -1;
	return 0;
}

static int __obj_compar(struct object *a, struct object *b)
{
	return __obj_compar_key(a, b->id);
}

static void _obj_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->tslock = SPINLOCK_INIT;
	obj->pagecache_root = RBINIT;
	obj->pagecache_level1_root = RBINIT;
	obj->tstable_root = RBINIT;
	obj->page_requests_root = RBINIT;
}

void obj_init(struct object *obj)
{
	_obj_ctor(NULL, obj);
	obj->slot = NULL;
	obj->flags = 0;
	obj->id = 0;
	obj->kvmap = NULL;
	obj->kaddr = NULL;
	krc_init(&obj->refs);
	krc_init_zero(&obj->mapcount);
	arch_object_init(obj);
	obj->ties_root = RBINIT;
}

static void _obj_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	assert(krc_iszero(&obj->refs));
	assert(krc_iszero(&obj->mapcount));
}

static struct objpage *objpage_alloc(void)
{
	struct objpage *op = slabcache_alloc(&sc_objpage);
	op->flags = 0;
	op->page = NULL;
	return op;
}

void obj_system_init(void)
{
	slabcache_init(&sc_objs, "sc_objs", sizeof(struct object), _obj_ctor, _obj_dtor, NULL);
	slabcache_init(&sc_objpage, "sc_objpage", sizeof(struct objpage), NULL, NULL, NULL);
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

	obj_init(obj);
	obj->id = id;
	obj_kso_init(obj, ksot);

	obj_count++;

	return obj;
}

struct object *obj_create(uint128_t id, enum kso_type ksot)
{
	struct object *obj = __obj_alloc(ksot, id);
	/* TODO (major): check for duplicates */
	if(id) {
		spinlock_acquire_save(&objlock);
		rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
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
	rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
	spinlock_release_restore(&objlock);
}

static void obj_clone_cow(struct object *src, struct object *nobj)
{
	// printk("CLONE_COW " IDFMT " -> " IDFMT "\n", IDPR(src->id), IDPR(nobj->id));
	spinlock_acquire_save(&src->lock);
	arch_object_remap_cow(src);
	for(struct rbnode *node = rb_first(&src->pagecache_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		if(pg->page) {
			assert(pg->page->cowcount >= 1);
			pg->page->cowcount++;
			// pg->flags &= ~OBJPAGE_MAPPED;
			pg->flags |= OBJPAGE_COW;

			struct objpage *npg = objpage_alloc();
			npg->idx = pg->idx;
			// npg->flags = pg->flags;
			npg->flags = OBJPAGE_COW;
			npg->page = pg->page;

			rb_insert(&nobj->pagecache_root, npg, struct objpage, node, __objpage_compar);
		}
	}
	for(struct rbnode *node = rb_first(&src->pagecache_level1_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		if(pg->page) {
			pg->page->cowcount++;
			// pg->flags &= ~OBJPAGE_MAPPED;
			pg->flags |= OBJPAGE_COW;

			struct objpage *npg = objpage_alloc();
			npg->idx = pg->idx;
			// npg->flags = pg->flags;
			npg->page = pg->page;
			npg->flags = OBJPAGE_COW;
			// npg->flags &= ~OBJPAGE_MAPPED;

			rb_insert(&nobj->pagecache_level1_root, npg, struct objpage, node, __objpage_compar);
		}
	}

	spinlock_release_restore(&src->lock);
}

void obj_copy_pages(struct object *dest, struct object *src, size_t doff, size_t soff, size_t len)
{
	if(src) {
		spinlock_acquire_save(&src->lock);
		arch_object_remap_cow(src);
	}
	spinlock_acquire_save(&dest->lock);

	size_t first_idx = 0;
	size_t last_idx = ~0;

	if(src) {
		struct rbnode *fn = rb_first(&src->pagecache_root);
		struct rbnode *ln = rb_last(&src->pagecache_root);
		if(fn) {
			struct objpage *pg = rb_entry(fn, struct objpage, node);
			first_idx = pg->idx;
		}
		if(ln) {
			struct objpage *pg = rb_entry(ln, struct objpage, node);
			last_idx = pg->idx;
		}
	}

	// printk("copy: fn = %lx, ln = %lx\n", first_idx, last_idx);

	for(size_t i = 0; i < len / mm_page_size(0); i++) {
		size_t dst_idx = doff / mm_page_size(0) + i;

		struct rbnode *dnode =
		  rb_search(&dest->pagecache_root, dst_idx, struct objpage, node, __objpage_compar_key);
		if(dnode) {
			struct objpage *dp = rb_entry(dnode, struct objpage, node);
			if(dp->page) {
				if(dp->flags & OBJPAGE_COW) {
					spinlock_acquire_save(&dp->page->lock);
					if(dp->page->cowcount-- <= 1) {
						page_dealloc(dp->page, 0);
					}
					spinlock_release_restore(&dp->page->lock);
				} else {
					page_dealloc(dp->page, 0);
				}
				dp->page = NULL;
			}
			rb_delete(&dp->node, &dest->pagecache_root);
			slabcache_free(&sc_objpage, dp);
		}

		if(src) {
			size_t src_idx = soff / mm_page_size(0) + i;
			if(src_idx < first_idx)
				continue;
			if(src_idx > last_idx)
				break;
			/* TODO: check max src_idx and min, and optimize (don't bother lookup if we exceed
			 * these) */
			struct rbnode *node =
			  rb_search(&src->pagecache_root, src_idx, struct objpage, node, __objpage_compar_key);
			//	printk("considering page %ld: %p\n", src_idx, node);
			if(node) {
				struct objpage *pg = rb_entry(node, struct objpage, node);
				if(pg->page) {
					//		printk("   cow! %ld -> %ld\n", src_idx, dst_idx);
					assert(pg->page->cowcount >= 1);
					pg->page->cowcount++;
					// pg->flags &= ~OBJPAGE_MAPPED;
					pg->flags |= OBJPAGE_COW;

					struct objpage *npg = objpage_alloc();
					npg->idx = dst_idx;
					// npg->flags = pg->flags;
					npg->flags = OBJPAGE_COW;
					npg->page = pg->page;

					rb_insert(&dest->pagecache_root, npg, struct objpage, node, __objpage_compar);
				}
			}
		}
	}
	spinlock_release_restore(&dest->lock);
	if(src) {
		spinlock_release_restore(&src->lock);
	}
}

struct object *obj_create_clone(uint128_t id, objid_t srcid, enum kso_type ksot)
{
	struct object *src = obj_lookup(srcid, 0);
	if(src == NULL) {
		return NULL;
	}
	struct object *obj = __obj_alloc(ksot, id);

#if 0
	spinlock_acquire_save(&src->lock);
	for(struct rbnode *node = rb_first(&src->pagecache_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		if(pg->page) {
			struct page *np = page_alloc(pg->page->type, 0, pg->page->level);
			assert(pg->page->level == np->level);
			void *csrc = tmpmap_map_page(pg->page);
			void *cdest = tmpmap_map_page(np);
			/* TODO (perf): copy-on-write */
			memcpy(cdest, csrc, mm_page_size(pg->page->level));
			tmpmap_unmap_page(cdest);
			tmpmap_unmap_page(csrc);
			obj_cache_page(obj, pg->idx * mm_page_size(0), np);
		}
	}
	for(struct rbnode *node = rb_first(&src->pagecache_level1_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		if(pg->page) {
			struct page *np = page_alloc(pg->page->type, 0, pg->page->level);
			assert(pg->page->level == np->level);
			/* TODO (perf): copy-on-write */
			void *csrc = tmpmap_map_page(pg->page);
			void *cdest = tmpmap_map_page(np);
			memcpy(cdest, csrc, mm_page_size(pg->page->level));
			tmpmap_unmap_page(cdest);
			tmpmap_unmap_page(csrc);
			obj_cache_page(obj, pg->idx * mm_page_size(1), np);
		}
	}

	spinlock_release_restore(&src->lock);
#else
	obj_clone_cow(src, obj);
#endif
	obj_put(src);

	if(id) {
		spinlock_acquire_save(&objlock);
		rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
		spinlock_release_restore(&objlock);
	}
	return obj;
}

int obj_untie(struct object *parent, struct object *child)
{
	// printk("untying " IDFMT " -> " IDFMT "\n", IDPR(child->id), IDPR(parent->id));
	struct object *rel = NULL;
	spinlock_acquire_save(&parent->lock);

	struct rbnode *node =
	  rb_search(&parent->ties_root, child->id, struct object_tie, node, __objtie_compar_key);
	struct object_tie *tie;
	if(!node) {
		spinlock_release_restore(&parent->lock);
		return -ENOENT;
	}
	tie = rb_entry(node, struct object_tie, node);
	if(--tie->count == 0) {
		rb_delete(&tie->node, &parent->ties_root);
		rel = tie->child;
		slabcache_free(&sc_objtie, tie);
	}

	spinlock_release_restore(&parent->lock);
	obj_put(rel);
	return 0;
}

void obj_tie(struct object *parent, struct object *child)
{
	// printk("tying " IDFMT " -> " IDFMT "\n", IDPR(child->id), IDPR(parent->id));
	spinlock_acquire_save(&parent->lock);

	struct rbnode *node =
	  rb_search(&parent->ties_root, child->id, struct object_tie, node, __objtie_compar_key);
	struct object_tie *tie;
	if(!node) {
		tie = slabcache_alloc(&sc_objtie);
		krc_get(&child->refs);
		tie->child = child;
		tie->count = 1;
		rb_insert(&parent->ties_root, tie, struct object_tie, node, __objtie_compar);
	} else {
		tie = rb_entry(node, struct object_tie, node);
		tie->count++;
	}

	spinlock_release_restore(&parent->lock);
}

struct object *obj_lookup(uint128_t id, int flags)
{
	spinlock_acquire_save(&objlock);
	struct rbnode *node = rb_search(&obj_tree, id, struct object, node, __obj_compar_key);

	struct object *obj = node ? rb_entry(node, struct object, node) : NULL;
	if(node) {
		krc_get(&obj->refs);
		spinlock_release_restore(&objlock);

		spinlock_acquire_save(&obj->lock);
		if((obj->flags & OF_HIDDEN) && !(flags & OBJ_LOOKUP_HIDDEN)) {
			spinlock_release_restore(&obj->lock);
			obj_put(obj);
			return NULL;
		}
		spinlock_release_restore(&obj->lock);
	} else {
		spinlock_release_restore(&objlock);
	}
	return obj;
}

static void __obj_alloc_kernel_slot(struct object *obj)
{
	if(obj->kslot)
		return;
	struct slot *slot = slot_alloc();
#if CONFIG_DEBUG_OBJECT_SLOT
	printk("[slot]: allocated kslot %ld for " IDFMT " (%p %lx %d)\n",
	  slot->num,
	  IDPR(obj->id),
	  obj,
	  obj->flags,
	  obj->kso_type);
#endif
	krc_get(&obj->refs);
	slot->obj = obj;   /* above get */
	obj->kslot = slot; /* move ref */
}

void obj_alloc_kernel_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	__obj_alloc_kernel_slot(obj);
	spinlock_release_restore(&obj->lock);
}

#if 0
void obj_release_kernel_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(!obj->kslot) {
		spinlock_release_restore(&obj->lock);
		return;
	}

	struct slot *slot = obj->kslot;
	obj->kslot = NULL;
	spinlock_release_restore(&obj->lock);

	vm_kernel_unmap_object(obj);
	object_space_release_slot(slot);
	slot_release(slot);
}
#endif

void obj_release_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);

	assert(obj->slot);
	assert(obj->mapcount.count > 0);

	// printk("REL_SLOT " IDFMT "; mapcount=%ld\n", IDPR(obj->id), obj->mapcount.count);
	if(krc_put(&obj->mapcount)) {
		/* hit zero; release */
		struct slot *slot = obj->slot;
		obj->slot = NULL;
		/* this krc drop MUST not reduce the count to 0, because there must be a reference to obj
		 * coming into this function */
		object_space_release_slot(slot);
		slot_release(slot);
		bool tmp = krc_put(&obj->refs);
		if(tmp) {
			panic("tmp nonzero for object " IDFMT "\n", IDPR(obj->id));
		}
		assert(!tmp);
#if CONFIG_DEBUG_OBJECT_SLOT
		printk("MAPCOUNT ZERO: " IDFMT "; refs=%ld\n", IDPR(obj->id), obj->refs.count);
#endif
	}

	spinlock_release_restore(&obj->lock);
}

struct slot *obj_alloc_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(!obj->slot) {
		assert(obj->mapcount.count == 0);
		obj->slot = slot_alloc();
		krc_get(&obj->refs);
		obj->slot->obj = obj; /* above get */
	}

	krc_get(&obj->mapcount);

#if CONFIG_DEBUG_OBJECT_SLOT
	printk("[slot]: allocated uslot %ld for " IDFMT " (%p %lx %d), mapcount %ld\n",
	  obj->slot->num,
	  IDPR(obj->id),
	  obj,
	  obj->flags,
	  obj->kso_type,
	  obj->mapcount.count);
#endif
	krc_get(&obj->slot->rc); /* return */
	spinlock_release_restore(&obj->lock);
	return obj->slot;
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
	if(p->cowcount == 0) {
		p->cowcount = 1;
	}
	// p->cowcount = 1;
	page->flags &= ~OBJPAGE_MAPPED;
	if(p->cowcount > 1) {
		page->flags |= OBJPAGE_COW;
	}
	if(node == NULL)
		rb_insert(root, page, struct objpage, node, __objpage_compar);
	// arch_object_map_page(obj, page->page, page->idx);
	// page->flags |= OBJPAGE_MAPPED;
	spinlock_release_restore(&obj->lock);
}

#include <processor.h>
#include <twz/_sys.h>

static struct objpage *__obj_get_page(struct object *obj, size_t addr, bool alloc)
{
	size_t idx = addr / mm_page_size(1);
	struct objpage *lpage = NULL;
	struct rbnode *node =
	  rb_search(&obj->pagecache_level1_root, idx, struct objpage, node, __objpage_compar_key);
	if(node)
		lpage = rb_entry(node, struct objpage, node);
	// printk("OGP0\n");
	if(lpage && lpage->page->level == 1) {
		/* found a large page */
		krc_get(&lpage->refs);
		return lpage;
	}
	idx = addr / mm_page_size(0);
	struct objpage *page = NULL;
	node = rb_search(&obj->pagecache_root, idx, struct objpage, node, __objpage_compar_key);
	if(node)
		page = rb_entry(node, struct objpage, node);
	// printk("OGP1\n");
	if(page == NULL) {
		if(!alloc) {
			return NULL;
		}
		//	printk("OGPa0\n");
		int level = ((addr >= mm_page_size(1))) ? 0 : 0; /* TODO */
		struct page *pp = page_alloc(
		  (obj->flags & OF_PERSIST) ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE, PAGE_ZERO, level);
		//	printk("OGPa0.0\n");
		pp->cowcount = 1;
		page = objpage_alloc();
		page->page = pp;
		//	printk("OGPa0.1\n");
		// page->page =
		//  page_alloc(obj->persist ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE, PAGE_ZERO, level);
		page->idx = addr / mm_page_size(page->page->level);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_UC, PAGE_CACHE_UC);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_WB, PAGE_CACHE_WB);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_WT, PAGE_CACHE_WT);
		page->page->flags |= flag_if_notzero(obj->cache_mode & OC_CM_WC, PAGE_CACHE_WC);
		//	printk("OGPa1\n");
#if 0
		printk("adding page %ld: %d %d :: " IDFMT "\n",
		  page->idx,
		  page->page->level,
		  level,
		  IDPR(obj->id));
#endif
		krc_init_zero(&page->refs);
		rb_insert(page->page->level ? &obj->pagecache_level1_root : &obj->pagecache_root,
		  page,
		  struct objpage,
		  node,
		  __objpage_compar);
		//	printk("OGPa2\n");
	}
	krc_get(&page->refs);
	return page;
}

struct objpage *obj_get_page(struct object *obj, size_t addr, bool alloc)
{
	spinlock_acquire_save(&obj->lock);
	struct objpage *op = __obj_get_page(obj, addr, alloc);
	spinlock_release_restore(&obj->lock);
	return op;
}

static void _objpage_release(void *page)
{
	(void)page; /* TODO (major): implement */
}

static void _obj_release(void *_obj)
{
	struct object *obj = _obj;
#if CONFIG_DEBUG_OBJECT_LIFE
	printk("OBJ RELEASE: " IDFMT "\n", IDPR(obj->id));
#endif
	if(obj->flags & OF_DELETE) {
#if CONFIG_DEBUG_OBJECT_LIFE
		printk("FINAL DELETE object " IDFMT "\n", IDPR(obj->id));
#endif

		struct rbnode *n, *next;
		for(n = rb_first(&obj->ties_root); n; n = next) {
			next = rb_next(n);

			struct object_tie *tie = rb_entry(n, struct object_tie, node);
#if CONFIG_DEBUG_OBJECT_LIFE
			printk("UNTIE object " IDFMT "\n", IDPR(tie->child->id));
#endif
			rb_delete(&tie->node, &obj->ties_root);
			obj_put(tie->child);
			slabcache_free(&sc_objtie, tie);
		}

		// printk("FREEING OBJECT PAGES: %d, " IDFMT "\n", obj->kso_type, IDPR(obj->id));
		// if(obj->kso_type)
		//	return;
		arch_object_unmap_all(obj);
		//	if(obj->kso_type)
		// return;
#if 1
		// spinlock_acquire_save(&obj->lock);
		for(struct rbnode *node = rb_first(&obj->pagecache_root); node; node = next) {
			struct objpage *pg = rb_entry(node, struct objpage, node);
			if(pg->page) {
				if(pg->flags & OBJPAGE_COW) {
					spinlock_acquire_save(&pg->page->lock);
					if(pg->page->cowcount-- <= 1) {
						page_dealloc(pg->page, 0);
					}
					spinlock_release_restore(&pg->page->lock);
				} else {
					page_dealloc(pg->page, 0);
				}
				pg->page = NULL;
			}
			next = rb_next(node);
			rb_delete(&pg->node, &obj->pagecache_root);
			slabcache_free(&sc_objpage, pg);
		}
		for(struct rbnode *node = rb_first(&obj->pagecache_level1_root); node; node = next) {
			struct objpage *pg = rb_entry(node, struct objpage, node);
			if(pg->page) {
				if(pg->flags & OBJPAGE_COW) {
					spinlock_acquire_save(&pg->page->lock);
					if(pg->page->cowcount-- <= 1) {
						page_dealloc(pg->page, 0);
					}
					spinlock_release_restore(&pg->page->lock);
				} else {
					page_dealloc(pg->page, 0);
				}
				pg->page = NULL;
			}
			next = rb_next(node);
			rb_delete(&pg->node, &obj->pagecache_level1_root);
			slabcache_free(&sc_objpage, pg);
		}

		arch_object_destroy(obj);
		obj_count--;
		// spinlock_release_restore(&obj->lock);
		//	printk("OK\n");
#endif

		/* TODO: clean up... */
		slabcache_free(&sc_objs, obj);
	}
}

void obj_put_page(struct objpage *p)
{
	krc_put_call(p, refs, _objpage_release);
}

void obj_put(struct object *o)
{
	if(krc_put_locked(&o->refs, &objlock)) {
		if(o->flags & OF_DELETE) {
			rb_delete(&o->node, &obj_tree);
		}
		spinlock_release_restore(&objlock);
		// workqueue_insert(&current_processor->wq, &o->delete_task, _obj_release, o);
		_obj_release(o);
	}
}

void *obj_get_kaddr(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);

	if(obj->kaddr == NULL) {
		assert(krc_iszero(&obj->kaddr_count));
		__obj_alloc_kernel_slot(obj);
		if(!obj->kvmap) {
			vm_kernel_map_object(obj);
		}
		obj->kaddr = (char *)SLOT_TO_VADDR(obj->kvmap->slot);
	}

	krc_get(&obj->kaddr_count);

	spinlock_release_restore(&obj->lock);

	return obj->kaddr;
}

void obj_free_kaddr(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(obj->kaddr == NULL) {
		spinlock_release_restore(&obj->lock);
		return;
	}
	krc_init_zero(&obj->kaddr_count);
	struct slot *slot = obj->kslot;
	obj->kslot = NULL;
	obj->kaddr = NULL;

	vm_kernel_unmap_object(obj);
	spinlock_release_restore(&obj->lock);
	struct object *o = slot->obj;
	assert(o == obj);
	slot->obj = NULL;
	object_space_release_slot(slot);
	slot_release(slot);
	obj_put(o);
}

bool obj_kaddr_valid(struct object *obj, void *kaddr, size_t run)
{
	void *ka = obj_get_kaddr(obj);
	bool ok = (kaddr >= ka && ((char *)kaddr + run) < (char *)ka + OBJ_MAXSIZE);
	obj_release_kaddr(obj);
	return ok;
}

void obj_release_kaddr(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(obj->kaddr == NULL) {
		spinlock_release_restore(&obj->lock);
		return;
	}
	if(krc_put(&obj->kaddr_count)) {
		/* TODO: but make these reclaimable */
		if(obj->kso_type)
			goto done;
		// goto done;
		struct slot *slot = obj->kslot;
		obj->kslot = NULL;
		obj->kaddr = NULL;

		vm_kernel_unmap_object(obj);
		spinlock_release_restore(&obj->lock);
		struct object *o = slot->obj;
		assert(o == obj);
		slot->obj = NULL;
		obj_put(o);
		object_space_release_slot(slot);
		slot_release(slot);
	}
done:
	spinlock_release_restore(&obj->lock);
}

/* TODO (major): these can probably fail */
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);

#if 0

	// printk(":: reading %lx -> %lx\n", start, start + len);
	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);
#endif
	void *addr = (char *)obj_get_kaddr(obj) + start;
	// void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + start;
	memcpy(ptr, addr, len);
	obj_release_kaddr(obj);
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);

#if 0
	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);
#endif

	void *addr = (char *)obj_get_kaddr(obj) + start;

	// void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + start;
	memcpy(addr, ptr, len);
	obj_release_kaddr(obj);
}

void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val)
{
	off += OBJ_NULLPAGE_SIZE;
	assert(off < OBJ_MAXSIZE && off + 8 <= OBJ_MAXSIZE);

#if 0
	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);
#endif

	void *addr = (char *)obj_get_kaddr(obj) + off;
	// void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + off;
	*(_Atomic uint64_t *)addr = val;
	obj_release_kaddr(obj);
}

bool obj_get_pflags(struct object *obj, uint32_t *pf)
{
	*pf = 0;
	spinlock_acquire_save(&obj->lock);
	if(obj->flags & OF_CPF_VALID) {
		*pf = obj->cached_pflags;
		spinlock_release_restore(&obj->lock);
		return true;
	}

	spinlock_release_restore(&obj->lock);
	struct metainfo mi;
	obj_read_data(obj, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	if(mi.magic != MI_MAGIC)
		return false;
	spinlock_acquire_save(&obj->lock);
	*pf = obj->cached_pflags = mi.p_flags;
	atomic_thread_fence(memory_order_seq_cst);
	obj->flags |= OF_CPF_VALID;
	spinlock_release_restore(&obj->lock);
	return true;
}

objid_t obj_compute_id(struct object *obj)
{
	void *kaddr = obj_get_kaddr(obj);
	struct metainfo *mi = (void *)((char *)kaddr + (OBJ_MAXSIZE - OBJ_METAPAGE_SIZE));

	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &mi->nonce, sizeof(mi->nonce));
	blake2b_update(&S, &mi->p_flags, sizeof(mi->p_flags));
	blake2b_update(&S, &mi->kuid, sizeof(mi->kuid));
	size_t tl = 0;
	atomic_thread_fence(memory_order_seq_cst);
	if(mi->p_flags & MIP_HASHDATA) {
		for(size_t s = 0; s < mi->sz; s += mm_page_size(0)) {
			size_t rem = mm_page_size(0);
			if(s + mm_page_size(0) > mi->sz) {
				rem = mi->sz - s;
			}
			assert(rem <= mm_page_size(0));

			void *addr = (char *)kaddr + s + OBJ_NULLPAGE_SIZE;
			blake2b_update(&S, addr, rem);
			tl += rem;
		}
		size_t mdbottom = OBJ_METAPAGE_SIZE + sizeof(struct fotentry) * mi->fotentries;
		size_t pos = OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + mdbottom);
		size_t thispage = mm_page_size(0);
		for(size_t s = pos; s < OBJ_MAXSIZE - OBJ_NULLPAGE_SIZE; s += thispage) {
			size_t offset = pos % mm_page_size(0);
			size_t len = mm_page_size(0) - offset;
			void *addr = (char *)kaddr + s + OBJ_NULLPAGE_SIZE;
			blake2b_update(&S, (char *)addr, len);
			tl += len;
		}
	}

	unsigned char tmp[32];
	blake2b_final(&S, tmp, 32);
	_Alignas(16) unsigned char out[16];
	for(int i = 0; i < 16; i++) {
		out[i] = tmp[i] ^ tmp[i + 16];
	}

	obj_release_kaddr(obj);

#if 0
	printk("computed ID tl=%ld " IDFMT " for object " IDFMT "\n",
	  tl,
	  IDPR(*(objid_t *)out),
	  IDPR(obj->id));
#endif
	return *(objid_t *)out;
}

bool obj_verify_id(struct object *obj, bool cache_result, bool uncache)
{
	/* avoid the lock here when checking for KERNEL because this flag is set during creation and
	 * never unset */
	if(obj->id == 0 || (obj->flags & OF_KERNEL))
		return true;
	bool result = false;
	spinlock_acquire_save(&obj->lock);

	if(obj->flags & OF_IDSAFE) {
		result = !!(obj->flags & OF_IDCACHED);
	} else {
		spinlock_release_restore(&obj->lock);
		objid_t c = obj_compute_id(obj);
		spinlock_acquire_save(&obj->lock);
		result = c == obj->id;
		obj->flags |= result && cache_result ? OF_IDCACHED : 0;
	}
	if(uncache) {
		obj->flags &= ~OF_IDSAFE;
	} else {
		obj->flags |= OF_IDSAFE;
	}
	spinlock_release_restore(&obj->lock);
	return result;
}

#include <processor.h>
#include <secctx.h>
#include <thread.h>

int obj_check_permission(struct object *obj, uint64_t flags)
{
	// printk("Checking permission of object %p: " IDFMT "\n", obj, IDPR(obj->id));
	bool w = (flags & MIP_DFL_WRITE);
	if(!obj_verify_id(obj, !w, w)) {
		return -EINVAL;
	}

	uint32_t p_flags;
	if(!obj_get_pflags(obj, &p_flags))
		return 0;
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);

	if((dfl & flags) == flags) {
		return 0;
	}
	return secctx_check_permissions((void *)arch_thread_instruction_pointer(), obj, flags);
}

#include <twz/_sctx.h>
static uint32_t __conv_objperm_to_scp(uint64_t p)
{
	uint32_t perms = 0;
	if(p & OBJSPACE_READ) {
		perms |= SCP_READ;
	}
	if(p & OBJSPACE_WRITE) {
		perms |= SCP_WRITE;
	}
	if(p & OBJSPACE_EXEC_U) {
		perms |= SCP_EXEC;
	}
	return perms;
}

static uint64_t __conv_scp_to_objperm(uint32_t p)
{
	uint64_t perms = 0;
	if(p & SCP_READ) {
		perms |= OBJSPACE_READ;
	}
	if(p & SCP_WRITE) {
		perms |= OBJSPACE_WRITE;
	}
	if(p & SCP_EXEC) {
		perms |= OBJSPACE_EXEC_U;
	}
	return perms;
}

static bool __objspace_fault_calculate_perms(struct object *o,
  uint32_t flags,
  uintptr_t loaddr,
  uintptr_t vaddr,
  uintptr_t ip,
  uint64_t *perms)
{
	/* optimization: just check if default permissions are enough */
	uint32_t p_flags;
	if(!obj_get_pflags(o, &p_flags)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));

		return false;
	}
	uint32_t dfl = p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
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
		uint32_t res;
		if(secctx_fault_resolve(
		     (void *)ip, loaddr, (void *)vaddr, o, __conv_objperm_to_scp(flags), &res, true)
		   == -1) {
			return false;
		}
		*perms = __conv_scp_to_objperm(res);
	}

	bool w = (*perms & OBJSPACE_WRITE);
	if(!obj_verify_id(o, !w, w)) {
		struct fault_object_info info =
		  twz_fault_build_object_info(o->id, (void *)ip, (void *)vaddr, FAULT_OBJECT_INVALID);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		return false;
	}

	if(((*perms & flags) & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))
	   != (flags & (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U))) {
		panic("Insufficient permissions for mapping (should be handled earlier)");
	}
	return true;
}

struct object *obj_lookup_slot(uintptr_t oaddr, struct slot **slot)
{
	ssize_t tl = oaddr / mm_page_size(MAX_PGLEVEL);
	*slot = slot_lookup(tl);
	if(!*slot) {
		return NULL;
	}
	struct object *obj = (*slot)->obj;
	if(obj) {
		krc_get(&obj->refs);
	}
	return obj;
}

void kernel_objspace_fault_entry(uintptr_t ip, uintptr_t loaddr, uintptr_t vaddr, uint32_t flags)
{
	static size_t __c = 0;
	__c++;
	size_t idx = (loaddr % mm_page_size(MAX_PGLEVEL)) / mm_page_size(0);
	if(idx == 0 && !VADDR_IS_KERNEL(vaddr)) {
		struct fault_null_info info = twz_fault_build_null_info((void *)ip, (void *)vaddr);
		thread_raise_fault(current_thread, FAULT_NULL, &info, sizeof(info));
		return;
	}

	struct slot *slot;
	struct object *o = obj_lookup_slot(loaddr, &slot);

	if(o == NULL) {
		panic(
		  "no object mapped to slot during object fault: vaddr=%lx, oaddr=%lx, ip=%lx, slot=%ld",
		  vaddr,
		  loaddr,
		  ip,
		  loaddr / OBJ_MAXSIZE);
	}

	if(current_thread) {
		if(current_thread->_last_oaddr != loaddr || current_thread->_last_flags != flags) {
			current_thread->_last_oaddr = loaddr;
			current_thread->_last_flags = flags;
			current_thread->_last_count = 0;
		} else {
			current_thread->_last_count++;
			if(current_thread->_last_count > 50)
				panic("DOUBLE OADDR FAULT\n");
		}
	}
#if 0
	// uint64_t rsp;
	// asm volatile("mov %%rsp, %0" : "=r"(rsp));
	// printk("---> %lx\n", rsp);
	if(current_thread)
		printk("OSPACE FAULT %ld: ip=%lx loaddr=%lx (idx=%lx) vaddr=%lx flags=%x :: " IDFMT
		       " %lx\n",
		  current_thread ? current_thread->id : -1,
		  ip,
		  loaddr,
		  idx,
		  vaddr,
		  flags,
		  IDPR(o->id),
		  o->flags);
#endif

	uint64_t perms = 0;
	uint64_t existing_flags;

	bool do_map = !arch_object_getmap_slot_flags(NULL, slot, &existing_flags);
	do_map = do_map || (existing_flags & flags) != flags;

	// printk("A\n");

	// asm volatile("mov %%rsp, %0" : "=r"(rsp));
	// printk("---> %lx\n", rsp);
	if(do_map) {
		if(!VADDR_IS_KERNEL(vaddr) && !(o->flags & OF_KERNEL)) {
			if(!__objspace_fault_calculate_perms(o, flags, loaddr, vaddr, ip, &perms)) {
				goto done;
			}
			perms &= (OBJSPACE_READ | OBJSPACE_WRITE | OBJSPACE_EXEC_U);
		} else {
			perms = OBJSPACE_READ | OBJSPACE_WRITE;
		}
		if((flags & perms) != flags) {
			panic("TODO: this mapping will never work");
		}

		//	printk("B\n");
		spinlock_acquire_save(&slot->lock);
		if(!arch_object_getmap_slot_flags(NULL, slot, &existing_flags)) {
			//		if(o->flags & OF_KERNEL)
			//			arch_object_map_slot(NULL, o, slot, perms);
			//		else
			object_space_map_slot(NULL, slot, perms);
		} else if((existing_flags & flags) != flags) {
			arch_object_map_slot(NULL, o, slot, perms);
		}
		//	printk("C\n");
		spinlock_release_restore(&slot->lock);
	}

	if(o->flags & OF_ALLOC) {
		struct objpage p = { 0 };
		//	printk("X\n");
		p.page = page_alloc(PAGE_TYPE_VOLATILE,
		  (current_thread && current_thread->page_alloc) ? PAGE_CRITICAL : 0,
		  0); /* TODO: refcount, largepage */
		p.idx = (loaddr % OBJ_MAXSIZE) / mm_page_size(p.page->level);
		p.page->flags = PAGE_CACHE_WB;
		//	printk("Y\n");
		spinlock_acquire_save(&p.page->lock);
		arch_object_map_page(o, &p);
		spinlock_release_restore(&p.page->lock);
	} else {
		//	printk("P\n");
		struct objpage *p = obj_get_page(o, loaddr % OBJ_MAXSIZE, !(o->flags & OF_PAGER));
		if(!p) {
			assert(o->flags & OF_PAGER);
			kernel_queue_pager_request_page(o, (loaddr % OBJ_MAXSIZE) / mm_page_size(0));
			goto done;
		}
		assert(p);
		//	printk("P0\n");
		if(!(o->flags & OF_KERNEL))
			spinlock_acquire_save(&o->lock);
		//	printk("P1\n");
		if(!(o->flags & OF_KERNEL))
			spinlock_acquire_save(&p->page->lock);

#if 0
		printk(":: ofl=%lx, opfl=%lx, pcc=%d, %ld %ld\n",
		  o->flags,
		  p->flags,
		  p->page->cowcount,
		  o->refs.count,
		  o->mapcount.count);
#endif
		if(!(o->flags & OF_KERNEL)) {
			if(p->page->cowcount <= 1 && (p->flags & OBJPAGE_COW)) {
				p->flags &= ~(OBJPAGE_COW | OBJPAGE_MAPPED);
				p->page->cowcount = 1;
				//	printk("COW: reset %ld\n", p->idx);
			}

			if((p->flags & OBJPAGE_COW) && (flags & OBJSPACE_FAULT_WRITE)) {
				uint32_t old_count = atomic_fetch_sub(&p->page->cowcount, 1);
				//	printk("COW: copy %ld: %d\n", p->idx, old_count);

				if(old_count > 1) {
					struct page *np = page_alloc(p->page->type, 0, p->page->level);
					assert(np->level == p->page->level);
					np->cowcount = 1;
					void *cs = mm_ptov_try(p->page->addr);
					void *cd = mm_ptov_try(np->addr);

					bool src_fast = !!cs;
					bool dest_fast = !!cd;

					if(!cs) {
						cs = tmpmap_map_page(p->page);
					}
					if(!cd) {
						cd = tmpmap_map_page(np);
					}

					memcpy(cd, cs, mm_page_size(p->page->level));

					if(!dest_fast) {
						tmpmap_unmap_page(cd);
					}
					if(!src_fast) {
						tmpmap_unmap_page(cs);
					}

					assert(np->cowcount == 1);

					if(!(o->flags & OF_KERNEL))
						spinlock_release_restore(&p->page->lock);
					p->page = np;
					if(!(o->flags & OF_KERNEL))
						spinlock_acquire_save(&p->page->lock);
				} else {
					p->page->cowcount = 1;
				}

				p->flags &= ~OBJPAGE_COW;
				arch_object_map_page(o, p);
				p->flags |= OBJPAGE_MAPPED;
				// int x;
				/* TODO: better invalidation scheme */
				// asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));

				// for(;;)
				//	;
			} else {
				// printk("P: %p %lx %lx\n", p->page, p->page->addr, p->flags);
				if(!(p->flags & OBJPAGE_MAPPED)) {
					arch_object_map_page(o, p);
					p->flags |= OBJPAGE_MAPPED;
					// int x;
					/* TODO: better invalidation scheme */
					// asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
				}
			}
		} else {
			// bool m = arch_object_getmap_slot_flags(NULL, slot, &existing_flags);

			spinlock_acquire_save(&o->lock);
			if(!(p->flags & OBJPAGE_MAPPED)) {
				arch_object_map_page(o, p);
				p->flags |= OBJPAGE_MAPPED;
				// int x;
				/* TODO: better invalidation scheme */
				// asm volatile("invept %0, %%rax" ::"m"(x), "r"(0));
			}
			spinlock_release_restore(&o->lock);
		}
		if(!(o->flags & OF_KERNEL))
			spinlock_release_restore(&p->page->lock);
		if(!(o->flags & OF_KERNEL))
			spinlock_release_restore(&o->lock);
	}
	// printk("Mapped successfully\n");
	//	do_map = false;
	//}
	/* TODO: put page? */

done:

	// asm volatile("mov %%rsp, %0" : "=r"(rsp));
	// printk("---> %lx\n", rsp);
	obj_put(o);
	slot_release(slot);
}
