#include <arena.h>
#include <memory.h>
#include <page.h>
#include <slab.h>

#define PG_CRITICAL_THRESH 1024

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
_Atomic size_t mm_page_alloced = 0;
size_t mm_page_bootstrap_count = 0;

static struct arena page_arena;

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
	// printk("  alloc %p (%lx) %x from group %s\n", page, page->addr, page->flags, group->name);
	return page;
}

static void __do_page_dealloc(struct page_group *group, struct page *page)
{
	// printk("dealloc %p (%lx) %x to   group %s\n", page, page->addr, page->flags, group->name);
	if(!(page->flags & PAGE_ALLOCED)) {
		panic("tried to dealloc a non-allocated page");
	}
	page->flags &= ~PAGE_ALLOCED;
	spinlock_acquire_save(&group->lock);
	page->next = group->stack;
	group->stack = page;
	group->avail++;
	spinlock_release_restore(&group->lock);
}

void page_print_stats(void)
{
	printk("page bootstrap count: %ld (%ld KB; %ld MB)\n",
	  mm_page_bootstrap_count,
	  (mm_page_bootstrap_count * mm_page_size(0)) / 1024,
	  (mm_page_bootstrap_count * mm_page_size(0)) / (1024 * 1024));
	printk("page alloced: %ld (%ld KB; %ld MB)\n",
	  mm_page_alloced,
	  (mm_page_alloced * mm_page_size(0)) / 1024,
	  (mm_page_alloced * mm_page_size(0)) / (1024 * 1024));

	printk("page count: %ld (%ld KB; %ld MB)\n",
	  mm_page_count,
	  (mm_page_count * mm_page_size(0)) / 1024,
	  (mm_page_count * mm_page_size(0)) / (1024 * 1024));

	for(int i = 0; i < array_len(all_pgs); i++) {
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
			memset(mm_ptov(pages->addr), 0, mm_page_size(0));
			pages->level = 0;
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
		page_dealloc(page, 0);
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
	if(va) {
		memset(va, 0, mm_page_size(p->level));
		return;
	}
	// printk("!!\n");
	void *addr = tmpmap_map_page(p);
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

#define __PAGE_NONZERO 0x4000

static void __do_page_split(struct page_group *group)
{
	if(!group->split_fallback) {
		page_print_stats();
		panic("tried to split beyond page levels");
	}
	struct page *lp = __do_page_alloc(group->split_fallback);
	// printk("$$ %p %d %s\n", lp, group->level, group->name);
	if(!lp) {
		lp = page_alloc(PM_TYPE_DRAM /* TODO */, group->flags & PAGE_ZERO, group->level + 1);
	}
	if(!lp) {
		page_print_stats();
		panic("failed to split a larger page for group %s", group->name);
	}
	// printk("splitting page %lx (level %ld)\n", lp->addr, group->level + 1);
	for(size_t i = 0; i < mm_page_size(group->level + 1) / mm_page_size(group->level); i++) {
		struct page *np = arena_allocate(&page_arena, sizeof(struct page));
		np->type = lp->type;
		np->flags = lp->flags;
		np->lock = SPINLOCK_INIT;
		np->root = RBINIT;
		np->addr = i * mm_page_size(group->level) + lp->addr;
		np->parent = lp;
		np->next = NULL;
		np->level = group->level;

		if((group->flags & PAGE_ZERO) && !(np->flags & PAGE_ZERO)) {
			page_zero(np);
		}

		__do_page_dealloc(group, np);
	}
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

struct page *page_alloc(int type, int flags, int level)
{
	if(!mm_ready)
		level = 0;

	// printk("PAGE_ALLOC %x %d: 0\n", flags, level);
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

	struct page_group *pg = default_pg[level];

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
				__do_page_split(pg);
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
		//	panic("failed to allocate a zero page");
	}
	return p;
}

#if 0
struct page *page_alloc(int type, int flags, int level)
{
	if(!mm_ready)
		level = 0;
	struct page_stack *stack = &_stacks[level];
	spinlock_acquire_save(&stack->lock);
	bool zero = (flags & PAGE_ZERO);
	struct page *p = __do_page_alloc(stack, zero);
	if(!p) {
		if(mm_ready) {
			if(flags & __PAGE_NONZERO) {
				if(stack->adding) {
					spinlock_release_restore(&stack->lock);
					return NULL;
				}
				goto add;
			} else {
				if(flags & PAGE_ZERO) {
					//	printk("MANUALLY ZEROING\n");
				}
			}
			p = __do_page_alloc(stack, !zero);
			if(!p) {
				panic("out of pages; level=%d; avail=%ld, nzero=%ld, adding=%d; flags=%x",
				  level,
				  stack->avail,
				  stack->nzero,
				  stack->adding,
				  flags);
			}
			if(zero) {
				spinlock_release_restore(&stack->lock);
				page_zero(p);
				spinlock_acquire_save(&stack->lock);
			}
		} else {
			page_init_bootstrap();
			spinlock_release_restore(&stack->lock);
			return page_alloc(type, flags, level);
		}
	}

	if(stack->avail < 128 && level < MAX_PGLEVEL && mm_ready && !stack->adding
	   && !(flags & PAGE_CRITICAL)) {
	add:
		stack->adding = true;
		spinlock_release_restore(&stack->lock);
		struct page *lp = page_alloc(type, 0, level + 1);
		printk("splitting page %lx (level %d)\n", lp->addr, level + 1);
		for(size_t i = 0; i < mm_page_size(level + 1) / mm_page_size(level); i++) {
			struct page *np = arena_allocate(&page_arena, sizeof(struct page));
			//*np = *lp;
			np->type = lp->type;
			np->flags = lp->flags & ~PAGE_ALLOCED;
			np->lock = SPINLOCK_INIT;
			np->root = RBINIT;
			np->addr = i * mm_page_size(level) + lp->addr;
			// np->addr += i * mm_page_size(level);
			np->parent = lp;
			np->next = NULL;
			np->level = level;
			// printk("  %p -> %lx (%d)\n", np, np->addr, np->level);
			spinlock_acquire_save(&stack->lock);
			__do_page_dealloc(stack, np);
			spinlock_release_restore(&stack->lock);
		}
		stack->adding = false;
		return page_alloc(type, flags, level);
	} else {
		spinlock_release_restore(&stack->lock);
	}

	// printk(":: ALL %lx\n", p->addr);
	assert(!(p->flags & PAGE_ALLOCED));
	p->flags &= ~PAGE_ZERO; // TODO: track this using VM system
	p->flags |= PAGE_ALLOCED;
	p->cowcount = 0;
	mm_page_alloced++;
	return p;
}
#endif

struct page *page_alloc_nophys(void)
{
	struct page *page = arena_allocate(&page_arena, sizeof(struct page));
	return page;
}

#include <processor.h>
#if 0
static void __page_idle_zero(int level)
{
	struct page_stack *stack = &_stacks[level];
	while(((stack->nzero < stack->avail && stack->nzero < 1024) || stack->avail < 1024)
	      && !stack->adding) {
#if 1
		printk("ACTIVATE: idle zero: %d: %ld %ld %d\n",
		  level,
		  stack->avail,
		  stack->nzero,
		  processor_has_threads(current_processor));
#endif
		struct page *p = page_alloc(PAGE_TYPE_VOLATILE, __PAGE_NONZERO, level);
		if(p) {
			// page_zero(p);
			page_dealloc(p, PAGE_ZERO);
		}
		spinlock_acquire_save(&current_processor->sched_lock);
		bool br = processor_has_threads(current_processor);
		spinlock_release_restore(&current_processor->sched_lock);
		if(br)
			break;
		for(volatile int i = 0; i < 100; i++)
			arch_processor_relax();

		spinlock_acquire_save(&current_processor->sched_lock);
		br = processor_has_threads(current_processor);
		spinlock_release_restore(&current_processor->sched_lock);
		if(br)
			break;
	}
}
#endif
void page_idle_zero(void)
{
	static _Atomic int trying = 0;
	if(atomic_fetch_or(&trying, 1))
		return;
	// if(!processor_has_threads(current_processor)) {
	//__page_idle_zero(0);
	//}
	// if(!processor_has_threads(current_processor)) {
	//	__page_idle_zero(1);
	//}
	trying = 0;
}
