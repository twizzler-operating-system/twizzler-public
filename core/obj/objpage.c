#include <kalloc.h>
#include <lib/iter.h>
#include <lib/list.h>
#include <lib/rb.h>
#include <object.h>
#include <page.h>
#include <pager.h>
#include <processor.h>
#include <slab.h>

#include <twz/_sys.h>

static struct slabcache sc_objpage;

void obj_system_init_objpage(void)
{
	slabcache_init(&sc_objpage, "sc_objpage", sizeof(struct objpage), NULL, NULL, NULL);
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

static int __objpage_idxmap_compar_key(struct objpage *a, size_t n)
{
	if(a->srcidx > n)
		return 1;
	else if(a->srcidx < n)
		return -1;
	return 0;
}

static int __objpage_idxmap_compar(struct objpage *a, struct objpage *b)
{
	return __objpage_idxmap_compar_key(a, b->srcidx);
}

static struct objpage *objpage_alloc(struct object *obj)
{
	struct objpage *op = slabcache_alloc(&sc_objpage);
	op->flags = 0;
	op->srcidx = 0;
	op->page = NULL;
	op->lock = SPINLOCK_INIT;
	krc_init(&op->refs);
	op->obj = obj; /* weak */
	return op;
}

static void objpage_delete(struct objpage *op)
{
	if(op->page) {
		if(op->flags & OBJPAGE_COW) {
			if(op->page->cowcount-- <= 1) {
				page_dealloc(op->page, 0);
			}
		} else {
			page_dealloc(op->page, 0);
		}
		op->page = NULL;
	}
	slabcache_free(&sc_objpage, op);
}

void objpage_release(struct objpage *op, int flags)
{
	if(!(flags & OBJPAGE_RELEASE_OBJLOCKED)) {
		spinlock_acquire_save(&op->obj->lock);
	}
	spinlock_acquire_save(&op->lock);
	struct object *obj = op->obj;
	if(flags & OBJPAGE_RELEASE_UNMAP) {
		rb_delete(&op->node, &op->obj->pagecache_root);
		op->obj = NULL;
	}
	if(krc_put(&op->refs)) {
		objpage_delete(op);
	}
	spinlock_release_restore(&op->lock);
	if(!(flags & OBJPAGE_RELEASE_OBJLOCKED)) {
		spinlock_release_restore(&obj->lock);
	}
}

#define OBJPAGE_CLONE_REMAP 1

static struct objpage *objpage_clone(struct object *newobj, struct objpage *op, int flags)
{
	struct objpage *new_op = objpage_alloc(newobj);
	spinlock_acquire_save(&op->lock);
	if(op->page) {
		new_op->page = op->page;
		if(flags & OBJPAGE_CLONE_REMAP) {
			arch_object_page_remap_cow(op);
		}
		op->page->cowcount++;
	}
	op->flags |= OBJPAGE_COW;
	spinlock_release_restore(&op->lock);
	new_op->idx = op->idx;
	new_op->flags = OBJPAGE_COW;

	return new_op;
}

void obj_clone_cow(struct object *src, struct object *nobj)
{
	spinlock_acquire_save(&src->lock);
	arch_object_remap_cow(src);
	for(struct rbnode *node = rb_first(&src->pagecache_root); node; node = rb_next(node)) {
		struct objpage *pg = rb_entry(node, struct objpage, node);
		struct objpage *npg = objpage_clone(nobj, pg, 0);
		rb_insert(&nobj->pagecache_root, npg, struct objpage, node, __objpage_compar);
	}

	spinlock_release_restore(&src->lock);
}

void obj_copy_pages(struct object *dest, struct object *src, size_t doff, size_t soff, size_t len)
{
	if(dest->sourced_from && src) {
		panic("TODO: NI - object with pages sourced from multiple objects %p");
	}
	if(src) {
		spinlock_acquire_save(&src->lock);
		arch_object_remap_cow(src);
	}
	spinlock_acquire_save(&dest->lock);

#if 0
	printk("copy pages::" IDFMT " => " IDFMT ": %lx -> %lx for %lx\n",
	  IDPR(src ? src->id : 0),
	  IDPR(dest->id),
	  soff / 0x1000,
	  doff / 0x1000,
	  len / 0x1000);
#endif
	size_t first_idx = 0;
	size_t last_idx = ~0;

	if(src) {
		krc_get(&src->refs);
		dest->sourced_from = src;
		dest->flags |= OF_PARTIAL;

		struct derivation_info *di = kalloc(sizeof(*di));
		di->id = dest->id;
		list_insert(&src->derivations, &di->entry);

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

	for(size_t i = 0; i < len / mm_page_size(0); i++) {
		size_t dst_idx = doff / mm_page_size(0) + i;

		struct rbnode *dnode =
		  rb_search(&dest->pagecache_root, dst_idx, struct objpage, node, __objpage_compar_key);
		if(dnode) {
			struct objpage *dp = rb_entry(dnode, struct objpage, node);
			objpage_release(dp, OBJPAGE_RELEASE_UNMAP | OBJPAGE_RELEASE_OBJLOCKED);
		}

		if(src) {
			size_t src_idx = soff / mm_page_size(0) + i;
			//		if(src_idx < first_idx)
			//			continue;
			//		if(src_idx > last_idx)
			//			break;
			/* TODO: check max src_idx and min, and optimize (don't bother lookup if we exceed
			 * these) */
			struct rbnode *node =
			  rb_search(&src->pagecache_root, src_idx, struct objpage, node, __objpage_compar_key);
			struct objpage *new_op;
			if(node) {
				struct objpage *pg = rb_entry(node, struct objpage, node);
				new_op = objpage_clone(dest, pg, 0);
			} else {
				new_op = objpage_alloc(dest);
				new_op->srcidx = src_idx;
				rb_insert(
				  &dest->idx_map, new_op, struct objpage, idx_map_node, __objpage_idxmap_compar);
			}
			new_op->idx = dst_idx;
			rb_insert(&dest->pagecache_root, new_op, struct objpage, node, __objpage_compar);
		}
	}
	spinlock_release_restore(&dest->lock);
	if(src) {
		spinlock_release_restore(&src->lock);
	}
}

static struct objpage *__obj_get_large_page(struct object *obj, size_t addr)
{
	size_t idx = addr / mm_page_size(1);
	struct objpage *op = NULL;
	struct rbnode *node =
	  rb_search(&obj->pagecache_level1_root, idx, struct objpage, node, __objpage_compar_key);
	if(node) {
		op = rb_entry(node, struct objpage, node);
		krc_get(&op->refs);
		return op;
	}
	return NULL;
}

static void __obj_get_page_alloc_page(struct objpage *op)
{
	struct page *pp = page_alloc(
	  (op->obj->flags & OF_PERSIST) ? PAGE_TYPE_PERSIST : PAGE_TYPE_VOLATILE, PAGE_ZERO, 0);
	pp->cowcount = 1;
	op->page = pp;
	op->page->flags |= flag_if_notzero(op->obj->cache_mode & OC_CM_UC, PAGE_CACHE_UC);
	op->page->flags |= flag_if_notzero(op->obj->cache_mode & OC_CM_WB, PAGE_CACHE_WB);
	op->page->flags |= flag_if_notzero(op->obj->cache_mode & OC_CM_WT, PAGE_CACHE_WT);
	op->page->flags |= flag_if_notzero(op->obj->cache_mode & OC_CM_WC, PAGE_CACHE_WC);
}

static void __obj_get_page_handle_derived(struct objpage *op)
{
	/* okay, we're adding a page to an object. We need to check:
	 *   for each object with pages derived from this object, would we have cared about this page
	 * when deriving? This means,
	 *   - if the object is "whole-derived", check the page with the same idx as op.
	 *   - if the object is partially derived, look up the page in the src_idx -> dest_idx map for
	 *     derived object, and check that page.
	 */

	assert(op->page);
	assert(!(op->flags & OBJPAGE_MAPPED));
	struct object *obj = op->obj;
	foreach(e, list, &obj->derivations) {
		struct derivation_info *derived = list_entry(e, struct derivation_info, entry);

		struct object *derived_obj = obj_lookup(derived->id, 0);
		// printk("checking derivation object " IDFMT ": %d\n", IDPR(derived->id), !!derived_obj);
		if(!derived_obj)
			continue;

		spinlock_acquire_save(&derived_obj->lock);
		struct rbnode *node;
#if 0
		node = rb_search(
		  &derived_obj->pagecache_root, op->idx, struct objpage, node, __objpage_compar_key);
		// printk("  normal idx %lx returned %d\n", op->idx, !!node);
		if(node && 0) {
			struct objpage *dop = rb_entry(node, struct objpage, node);
			if(dop->page == NULL) {
				op->flags |= OBJPAGE_COW;
				assert(op->page->cowcount > 0);
				op->page->cowcount++;
				dop->flags = OBJPAGE_COW;
				dop->page = op->page;
			}
		}
#endif

		node = rb_search(&derived_obj->idx_map,
		  op->idx,
		  struct objpage,
		  idx_map_node,
		  __objpage_idxmap_compar_key);
		// printk("  mapped idx %lx returned %d\n", op->idx, !!node);
		if(node) {
			rb_delete(node, &derived_obj->idx_map);
			struct objpage *dop = rb_entry(node, struct objpage, idx_map_node);
			// printk("    :: %lx\n", dop->idx);
			if(dop->page == NULL) {
				op->flags |= OBJPAGE_COW;
				assert(op->page->cowcount > 0);
				op->page->cowcount++;
				dop->flags = OBJPAGE_COW;
				dop->page = op->page;
			}
		}
		spinlock_release_restore(&derived_obj->lock);

		/* TODO: this can lead to deadlock */
		obj_put(derived_obj);
		//		printk("  done\n");
	}
}

static void __obj_get_page_alloc(struct object *obj, size_t idx, struct objpage **result)
{
	struct objpage *page = objpage_alloc(obj);
	page->idx = idx;
	__obj_get_page_alloc_page(page);
	rb_insert(&obj->pagecache_root, page, struct objpage, node, __objpage_compar);
	*result = page;
}

static enum obj_get_page_result __obj_get_page(struct object *obj,
  size_t addr,
  struct objpage **result,
  int flags)
{
	*result = NULL;
	spinlock_acquire_save(&obj->lock);
	struct objpage *op = __obj_get_large_page(obj, addr);
	if(op) {
		*result = op;
		spinlock_release_restore(&obj->lock);
		return GETPAGE_OK;
	}
	objid_t bs = ((objid_t)0xE8F8615C6BEB8A00 << 64) | 0x44398226A06DBE62ul;
	size_t idx = addr / mm_page_size(0);
	if(obj->sourced_from && obj->sourced_from->id == bs) {
		// printk("lookup page for (derived from) bash: " IDFMT " %lx\n", IDPR(obj->id), idx);
	}
	struct rbnode *node;
	node = rb_search(&obj->pagecache_root, idx, struct objpage, node, __objpage_compar_key);
	if(node) {
		op = rb_entry(node, struct objpage, node);
		krc_get(&op->refs);
		*result = op;

		enum obj_get_page_result res = GETPAGE_OK;
		if(op->page == NULL) {
			if(op->obj->flags & OF_PAGER) {
				if(!(flags & OBJ_GET_PAGE_PAGEROK)) {
					panic("tried to get a page from a paged object without PAGEROK");
				}
				kernel_queue_pager_request_page(obj, idx);
				spinlock_release_restore(&obj->lock);
				res = GETPAGE_PAGER;
			} else if(obj->sourced_from) {
				struct objpage *sop;
				spinlock_release_restore(&obj->lock);

#if 0
				printk("%lx getting page %lx from source obj " IDFMT "\n",
				  idx,
				  op->srcidx,
				  IDPR(obj->sourced_from->id));
#endif
				/* TODO: we can probably pass flags & ~OBJ_GET_PAGE_ALLOC  and handle that. */
				assert(op->srcidx);
				res = obj_get_page(
				  obj->sourced_from, op->srcidx ? op->srcidx * mm_page_size(0) : addr, &sop, flags);
				if(res == GETPAGE_OK) {
					struct objpage *oldres = *result;
					res = obj_get_page(obj, addr, result, flags);
					if(oldres) {
						objpage_release(oldres, 0);
					}
				}
				if(sop) {
					objpage_release(sop, 0);
				}
			} else if(flags & OBJ_GET_PAGE_ALLOC) {
				__obj_get_page_alloc_page(op);
				__obj_get_page_handle_derived(op);
				spinlock_release_restore(&obj->lock);
			} else {
				spinlock_release_restore(&obj->lock);
			}
		} else {
			spinlock_release_restore(&obj->lock);
		}

		return res;
	}

	if(obj->sourced_from && obj->sourced_from->id == bs) {
		// printk("A\n");
	}
	if(obj->flags & OF_PAGER) {
		if(!(flags & OBJ_GET_PAGE_PAGEROK)) {
			panic("tried to get a page from a paged object without PAGEROK");
		}

		kernel_queue_pager_request_page(obj, idx);
		/* TODO: error ? */
		spinlock_release_restore(&obj->lock);
		return GETPAGE_PAGER;
	}

#if 0
	if(obj->sourced_from && !(obj->flags & OF_PARTIAL)) {
		struct objpage *sop;
		spinlock_release_restore(&obj->lock);
		enum obj_get_page_result res = obj_get_page(obj->sourced_from, addr, &sop, flags);
		printk("getting page %lx from source obj " IDFMT ": %d\n",
		  idx,
		  IDPR(obj->sourced_from->id),
		  res);
		if(res == GETPAGE_OK) {
			res = obj_get_page(obj, addr, result, flags);
		}
		if(sop) {
			objpage_release(sop, 0);
		}
		return res;
	}
#endif

	if(!(flags & OBJ_GET_PAGE_ALLOC)) {
		spinlock_release_restore(&obj->lock);
		return GETPAGE_NOENT;
	}

	__obj_get_page_alloc(obj, idx, result);
	__obj_get_page_handle_derived(*result);
	spinlock_release_restore(&obj->lock);
	return GETPAGE_OK;
}

enum obj_get_page_result obj_get_page(struct object *obj,
  size_t addr,
  struct objpage **result,
  int flags)
{
	enum obj_get_page_result r = __obj_get_page(obj, addr, result, flags);
	return r;
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
		page = objpage_alloc(obj);
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
	__obj_get_page_handle_derived(page);
	// arch_object_map_page(obj, page->page, page->idx);
	// page->flags |= OBJPAGE_MAPPED;
	spinlock_release_restore(&obj->lock);
}
