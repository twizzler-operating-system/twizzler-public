#include <lib/rb.h>
#include <object.h>
#include <page.h>
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
static struct objpage *objpage_alloc(void)
{
	struct objpage *op = slabcache_alloc(&sc_objpage);
	op->flags = 0;
	op->page = NULL;
	return op;
}

void obj_clone_cow(struct object *src, struct object *nobj)
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

void obj_put_page(struct objpage *p)
{
	krc_put_call(p, refs, _objpage_release);
}
