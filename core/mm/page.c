#include <arena.h>
#include <memory.h>
#include <page.h>
#include <processor.h>
#include <slab.h>

#define PG_CRITICAL_THRESH 1024

static DECLARE_PER_CPU(_Atomic bool, page_recur_crit_flag);

bool page_set_crit_flag(void)
{
	_Atomic bool *recur_flag = per_cpu_get(page_recur_crit_flag);
	return atomic_exchange(recur_flag, true);
}

void page_reset_crit_flag(bool v)
{
	_Atomic bool *recur_flag = per_cpu_get(page_recur_crit_flag);
	*recur_flag = v;
}

#if DO_PAGE_MAGIC
#define page_get_magic(p) ((p)->page_magic)
#else
#define page_get_magic(p) 0l
#endif

struct page_stack {
	struct spinlock lock, lock2;
	struct page *top;
	struct page *top_z;
	_Atomic size_t avail, nzero;
	_Atomic bool adding;
};

struct page_group {
	struct spinlock lock;
	_Atomic size_t avail;
	struct page *stack;
	const char *name;
	size_t level;
	struct page_group *split_fallback;
	struct page_group *fallback;
	int flags;
};

static struct page_group _pg_level2 = {
	.name = "pg-level2-normal",
	.lock = SPINLOCK_INIT,
	.level = 2,
	.split_fallback = NULL,
	.fallback = NULL,
};

static struct page_group _pg_level1 = {
	.name = "pg-level1-normal",
	.lock = SPINLOCK_INIT,
	.level = 1,
	.split_fallback = &_pg_level2,
	.fallback = NULL,
};

static struct page_group _pg_level1_zero = {
	.name = "pg-level1-zero",
	.lock = SPINLOCK_INIT,
	.level = 1,
	.split_fallback = &_pg_level2,
	.fallback = &_pg_level1,
	.flags = PAGE_ZERO,
};

static struct page_group _pg_level0_critical = {
	.name = "pg-level0-critical",
	.lock = SPINLOCK_INIT,
	.level = 0,
	.split_fallback = NULL,
	.fallback = NULL,
	.flags = PAGE_CRITICAL | PAGE_ZERO,
};

static struct page_group _pg_level0 = {
	.name = "pg-level0-normal",
	.lock = SPINLOCK_INIT,
	.level = 0,
	.split_fallback = &_pg_level1,
	.fallback = NULL,
};

static struct page_group _pg_level0_zero = {
	.name = "pg-level0-zero",
	.lock = SPINLOCK_INIT,
	.level = 0,
	.split_fallback = &_pg_level1_zero,
	.fallback = &_pg_level0,
	.flags = PAGE_ZERO,
};

static struct page_group *all_pgs[] = {
	&_pg_level0_critical,
	&_pg_level0_zero,
	&_pg_level0,
	&_pg_level1_zero,
	&_pg_level1,
	&_pg_level2,
};

static struct page_group
  *default_pg[] = { [0] = &_pg_level0_zero, [1] = &_pg_level1_zero, [2] = &_pg_level2 };

// struct page_stack _stacks[MAX_PGLEVEL + 1];

size_t mm_page_count = 0;
size_t mm_page_alloc_count = 0;
_Atomic size_t mm_page_free = 0;
size_t mm_page_bootstrap_count = 0;

static struct arena page_arena;

#include <twz/driver/memory.h>

static void __do_page_build_stats(struct page_stats *stats, struct page_group *pg)
{
	stats->page_size = mm_page_size(pg->level);
	stats->avail = pg->avail;
	uint64_t info = 0;
	if(pg->flags & PAGE_CRITICAL)
		info |= PAGE_STATS_INFO_CRITICAL;
	if(pg->flags & PAGE_ZERO)
		info |= PAGE_STATS_INFO_ZERO;
	stats->info = info;
}

int page_build_stats(struct page_stats *stats, int idx)
{
	switch(idx) {
		case 0:
			__do_page_build_stats(stats, &_pg_level0_critical);
			break;
		case 1:
			__do_page_build_stats(stats, &_pg_level0_zero);
			break;
		case 2:
			__do_page_build_stats(stats, &_pg_level0);
			break;
		case 3:
			__do_page_build_stats(stats, &_pg_level1_zero);
			break;
		case 4:
			__do_page_build_stats(stats, &_pg_level1);
			break;
		case 5:
			__do_page_build_stats(stats, &_pg_level2);
			break;
		default:
			return -1;
	}
	return 0;
}

static struct page *__do_page_alloc(struct page_group *group)
{
	spinlock_acquire_save(&group->lock);
	struct page *page = group->stack;
	if(!page) {
		spinlock_release_restore(&group->lock);
		return NULL;
	}
	if(page->flags & PAGE_ALLOCED) {
		panic("tried to alloc an allocated page");
	}
	assert(group->avail > 0);
	group->stack = group->stack->next;
	group->avail--;
	spinlock_release_restore(&group->lock);
	page->flags |= PAGE_ALLOCED;
	mm_page_free -= mm_page_size(page->level) / mm_page_size(0);

	if(page->addr == 0) {
		panic("tried to alloc page addr 0 (%p; %x, %d, %d: %lx)",
		  page,
		  page->flags,
		  page->level,
		  page->type,
		  page_get_magic(page));
	}
	// printk("  alloc %p (%lx) %x from group %s\n", page, page->addr, page->flags, group->name);
	return page;
}

static void __do_page_dealloc(struct page_group *group, struct page *page)
{
	// printk("dealloc %p (%lx) %x to   group %s\n", page, page->addr, page->flags, group->name);
	if(!(page->flags & PAGE_ALLOCED)) {
		panic("tried to dealloc a non-allocated page");
	}
	if(page->addr == 0) {
		panic("tried to dealloc page addr 0");
	}
	page->flags &= ~PAGE_ALLOCED;
	spinlock_acquire_save(&group->lock);
	page->next = group->stack;
	group->stack = page;
	group->avail++;
	spinlock_release_restore(&group->lock);
	mm_page_free += mm_page_size(page->level) / mm_page_size(0);
}

void page_print_stats(void)
{
	printk("page bootstrap count: %ld (%ld KB; %ld MB)\n",
	  mm_page_bootstrap_count,
	  (mm_page_bootstrap_count * mm_page_size(0)) / 1024,
	  (mm_page_bootstrap_count * mm_page_size(0)) / (1024 * 1024));
	printk("pages free: %ld (%ld KB; %ld MB)\n",
	  mm_page_free,
	  (mm_page_free * mm_page_size(0)) / 1024,
	  (mm_page_free * mm_page_size(0)) / (1024 * 1024));

	printk("page count: %ld (%ld KB; %ld MB)\n",
	  mm_page_count,
	  (mm_page_count * mm_page_size(0)) / 1024,
	  (mm_page_count * mm_page_size(0)) / (1024 * 1024));

	for(unsigned i = 0; i < array_len(all_pgs); i++) {
		printk("page stack %-20s: %ld avail (%ld KB); flags = %x\n",
		  all_pgs[i]->name,
		  all_pgs[i]->avail,
		  all_pgs[i]->avail * mm_page_size(all_pgs[i]->level) / 1024,
		  all_pgs[i]->flags);
	}
	/*
	for(int i = 0; i <= MAX_PGLEVEL; i++) {
	    printk(
	      "page stack %d (%ld KB): %ld available\n", i, mm_page_size(i) / 1024, _stacks[i].avail);
	}*/
}

void page_init_bootstrap(void)
{
	/* bootstrap page allocator */
	for(int c = 0; c < 128; c++) {
		struct page *pages = mm_virtual_early_alloc();
		size_t nrpages = mm_page_size(0) / sizeof(struct page);
		static bool did_init = false;
		if(!did_init) {
			arena_create(&page_arena);
			did_init = true;
		}

		for(size_t i = 0; i < nrpages; i++, pages++) {
			pages->addr = mm_physical_early_alloc();
			assert(pages->addr != 0);
			memset(mm_ptov(pages->addr), 0, mm_page_size(0));
			pages->level = 0;
#if DO_PAGE_MAGIC
			pages->page_magic = PAGE_MAGIC;
#endif
			pages->flags = PAGE_CACHE_WB | PAGE_ZERO | PAGE_ALLOCED;
			__do_page_dealloc(&_pg_level0_critical, pages);
			// pages->next = _stacks[0].top;
			//_stacks[0].top = pages;
			//_stacks[0].avail++;
			mm_page_bootstrap_count++;
		}
	}
}

void page_init(struct memregion *region)
{
	uintptr_t addr = region->start;
	if(addr == 0)
		addr += mm_page_size(0);
	printk("[mm] init region %lx -> %lx (%ld KB; %ld MB)\n",
	  region->start,
	  region->start + region->length,
	  region->length / 1024,
	  region->length / (1024 * 1024));
	size_t nrpages = mm_page_size(0) / sizeof(struct page);
	size_t i = 0;
	struct page *pages = mm_memory_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
	while(addr < region->length + region->start) {
		int level = MAX_PGLEVEL;
		for(; level > 0; level--) {
			if(align_up(addr, mm_page_size(level)) == addr
			   && addr + mm_page_size(level) <= region->start + region->length) {
				break;
			}
		}

		// printk("addr: %lx -> %lx ; %d\n", addr, addr + mm_page_size(level), level);
		struct page *page = &pages[i];

		page->level = level;
		page->addr = addr;
		page->flags = PAGE_CACHE_WB | PAGE_ALLOCED;
		page->parent = NULL;
#if DO_PAGE_MAGIC
		page->page_magic = PAGE_MAGIC;
#endif

		if(page->addr) {
			/* don't make address 0 available. */
			page_dealloc(page, 0);
		}
		mm_page_count += mm_page_size(level) / mm_page_size(0);

		if(++i >= nrpages) {
			pages = mm_memory_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
			i = 0;
			mm_page_alloc_count += mm_page_size(0);
		}

		addr += mm_page_size(level);
	}
}

#include <tmpmap.h>
static void page_zero(struct page *p)
{
	void *va = mm_ptov_try(p->addr);
	if(va == 0xfffffffffffffffful) {
		panic("SAD! :: %lx\n", p->addr);
	}
	if(va) {
		memset(va, 0, mm_page_size(p->level));
		p->flags |= PAGE_ZERO;
		return;
	}
	// printk("!!\n");
	void *addr = tmpmap_map_page(p);

	if(addr == 0xfffffffffffffffful) {
		panic("SAD!2 :: %lx\n", p->addr);
	}
	memset(addr, 0, mm_page_size(p->level));
	tmpmap_unmap_page(addr);
	p->flags |= PAGE_ZERO;
}

/* deallocation strategy:
 *   - start with the default group for the level (which is zeroed except for top level)
 *   - if the critical pool is not large enough, store the page there, zeroing if necessary.
 *   - if the page is zeroed and the group is zeroed, store it there.
 *   - if the page is zeroed and the group is NOT zeroed, store it there anyway.
 *   - if the page is not zeroed and the group is zeroed, goto fallback group
 *   - if the page is not zeroed and the group is not zeroed, store it there.
 */

void page_dealloc(struct page *p, int flags)
{
	assert(p);
	if(p->flags & PAGE_NOPHYS) {
		panic("tried to delloc a nophys page");
	}
	if((flags & PAGE_ZERO) && !(p->flags & PAGE_ZERO)) {
		page_zero(p);
	}
	if(p->level == 0 && _pg_level0_critical.avail < PG_CRITICAL_THRESH) {
		if(!(p->flags & PAGE_ZERO)) {
			if(!mm_ready) {
				page_print_stats();
				panic("needed to zero a page before mm was ready; increase number of critical "
				      "pages reserved during init");
			}
			page_zero(p);
		}
		__do_page_dealloc(&_pg_level0_critical, p);
		return;
	}

	assert(p->level <= 2 && p->level >= 0);
	struct page_group *pg = default_pg[p->level];
	do {
		if(!(p->flags & PAGE_ZERO) && (pg->flags & PAGE_ZERO)) {
			pg = pg->fallback;
		} else {
			break;
		}
	} while(pg);

	if(!pg) {
		page_print_stats();
		panic("failed to find a page group to deallocate page");
	}

	assert(pg->level == p->level);

	if(!(p->flags & PAGE_ZERO) && (pg->flags & PAGE_ZERO)) {
		page_zero(p);
	}
	__do_page_dealloc(pg, p);
}

static bool __do_page_split(struct page_group *group, bool simple)
{
	if(!group->split_fallback) {
		page_print_stats();
		panic("tried to split beyond page levels");
	}
	struct page *lp = __do_page_alloc(group->split_fallback);
	// printk("$$ %p %d %s\n", lp, group->level, group->name);
	if(!lp) {
		if(simple)
			return false;
		lp = page_alloc(PM_TYPE_DRAM /* TODO */,
		  (group->flags & PAGE_ZERO)
		    | ((current_thread && current_thread->page_alloc) ? PAGE_CRITICAL : 0),
		  group->level + 1);
	}
	if(!lp) {
		page_print_stats();
		panic("failed to split a larger page for group %s", group->name);
	}
	// printk("splitting page %lx (level %ld)\n", lp->addr, group->level + 1);
	for(size_t i = 0; i < mm_page_size(group->level + 1) / mm_page_size(group->level); i++) {
		struct page *np = arena_allocate(&page_arena, sizeof(struct page));
#if DO_PAGE_MAGIC
		np->page_magic = PAGE_MAGIC;
#endif
		np->type = lp->type;
		np->flags = lp->flags;
		np->lock = SPINLOCK_INIT;
		// np->root = RBINIT;
		np->addr = i * mm_page_size(group->level) + lp->addr;
		assert(np->addr);
		np->parent = lp;
		np->next = NULL;
		np->level = group->level;

		if((group->flags & PAGE_ZERO) && !(np->flags & PAGE_ZERO)) {
			page_zero(np);
		}

		__do_page_dealloc(group, np);
	}
	return true;
}

/* allocation strategy:
 *   - if critical, return from critical pool.
 *   - start with default group
 *   - if req zero and group zero, return from group
 *   - if req zero and group not zero, goto fallback group
 *   - if req not zero and group not zero, ret from group
 *   - if req not zero and group not zero, check availability of fallback
 *     - if fallback has pages, goto fallback
 *     - else, return from group.
 *
 *   when returning from group,
 *   - if a group is "low" on pages, mark it as needing more pages.
 *   - if a group is out of pages, split from split_fallback.
 */

struct page *page_alloc(int type __unused, int flags, int level)
{
	struct page_group *pg = NULL;
	if(!mm_ready) {
		level = 0;
		flags |= PAGE_CRITICAL;
	}
	_Atomic bool *recur_flag = per_cpu_get(page_recur_crit_flag);
	if(current_thread) {
		if(current_thread->page_alloc && !(flags & PAGE_CRITICAL)) {
			page_print_stats();
			panic("PAGE_CRITICAL must be set when faulting during page_alloc");
		}
		current_thread->page_alloc = true;
	}
	bool rf = atomic_exchange(recur_flag, true);
	if(rf) {
		flags |= PAGE_CRITICAL;
	}
	struct page *p = NULL;
	if(flags & PAGE_CRITICAL) {
		p = __do_page_alloc(&_pg_level0_critical);
		if(!p) {
			page_print_stats();
			panic("out of critical pages");
		}
		assert(p->flags & PAGE_ZERO);
		goto done;
	}

	assert(level <= MAX_PGLEVEL);
	assert(level >= 0 && level <= 2);
	pg = default_pg[level];

	// printk("PAGE_ALLOC %x %d: 1\n", flags, level);
	do {
		//	printk("considering group %s: %x %x\n", pg->name, flags, level);
		if((pg->flags & PAGE_ZERO) == (flags & PAGE_ZERO)) {
			break;
		} else if(!(pg->flags & PAGE_ZERO) && (flags & PAGE_ZERO)) {
			pg = pg->fallback;
		} else {
			struct page_group *fb = pg->fallback;
			if(fb && fb->avail > 0) {
				pg = fb;
			} else {
				break;
			}
		}
	} while(pg);
	if(pg) {
		//	printk("PAGE_ALLOC %x %d: 2: pg=%s\n", flags, level, pg->name);
		while(!(p = __do_page_alloc(pg))) {
			if(pg->fallback) {
				//			printk("choosing to move to fallback %s\n", pg->fallback->name);
				pg = pg->fallback;
			} else {
				//			printk("choosing to split page\n");
				//		page_print_stats();
				__do_page_split(pg, false);
			}
		}
	}
done:
	// printk("PAGE_ALLOC %x %d: 3\n", flags, level);
	if(!p) {
		page_print_stats();
		panic("failed to allocate a page (found group=%s; flags=%x, level=%d)",
		  pg ? pg->name : "(none)",
		  flags,
		  level);
	}
	if((flags & PAGE_ZERO) && !(p->flags & PAGE_ZERO)) {
		page_zero(p);
	}
	if(current_thread)
		current_thread->page_alloc = false;
	*recur_flag = rf;
	return p;
}

struct page *page_alloc_nophys(void)
{
	_Atomic bool *recur_flag = per_cpu_get(page_recur_crit_flag);
	bool old = *recur_flag;
	*recur_flag = true;
	struct page *page = arena_allocate(&page_arena, sizeof(struct page));
	*recur_flag = old;
#if DO_PAGE_MAGIC
	page->page_magic = PAGE_MAGIC;
#endif
	page->flags |= PAGE_NOPHYS;
	return page;
}

#include <processor.h>

size_t PG_ZERO_THRESH[] = {
	[0] = 4096, // 16MB
	[1] = 8,    // 16MB
	[2] = 0,
};

void page_idle_zero(void)
{
	_Atomic bool *recur_flag = per_cpu_get(page_recur_crit_flag);
	bool rf = atomic_exchange(recur_flag, true);

	struct page_group *crit = &_pg_level0_critical;
	while(crit->avail < PG_CRITICAL_THRESH) {
		if(processor_has_threads(current_processor))
			break;
		struct page *p = page_alloc(PM_TYPE_DRAM, PAGE_ZERO, 0);
		__do_page_dealloc(crit, p);
	}
	for(unsigned i = 0; i < array_len(all_pgs); i++) {
		if(processor_has_threads(current_processor))
			break;
		struct page_group *group = all_pgs[i];
		struct page_group *fb = group->fallback;
		struct page_group *sp = group->split_fallback;
		if((group->flags & PAGE_ZERO) && (sp || fb)) {
			while(group->avail < PG_ZERO_THRESH[group->level]) {
				if(processor_has_threads(current_processor))
					break;
				for(int x = 0; x < 100; x++) {
					arch_processor_relax();
				}
				struct page *page;
				if(fb && fb->avail) {
					if((page = __do_page_alloc(fb))) {
						assert(page->level == group->level);
						if(!(page->flags & PAGE_ZERO)) {
							page_zero(page);
						}
						// printk("piz: %s: moving page from fallback\n", group->name);
						__do_page_dealloc(group, page);
					} else {
						break;
					}
				} else if(sp) {
					*recur_flag = true;
					if(!__do_page_split(group, true)) {
						break;
					}
					*recur_flag = rf;
					// printk("piz: %s: splitting page\n", group->name);
				} else {
					break;
				}
			}
		} else if(!(group->flags & PAGE_ZERO) && sp) {
			*recur_flag = true;
			while(group->avail < PG_ZERO_THRESH[group->level]) {
				if(processor_has_threads(current_processor))
					break;
				for(int x = 0; x < 100; x++) {
					arch_processor_relax();
				}
				if(!__do_page_split(group, true))
					break;
			}
			*recur_flag = rf;
		}
	}

	*recur_flag = rf;
}
