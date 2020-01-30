#include <arena.h>
#include <memory.h>
#include <page.h>
#include <slab.h>

struct page_stack {
	struct spinlock lock, lock2;
	struct page *top;
	_Atomic size_t avail;
	_Atomic bool adding;
};

struct page_stack _stacks[MAX_PGLEVEL + 1];

size_t mm_page_count = 0;
size_t mm_page_alloc_count = 0;
size_t mm_page_alloced = 0;
size_t mm_page_bootstrap_count = 0;

static struct arena page_arena;

void page_init_bootstrap(void)
{
	/* bootstrap page allocator */
	struct page *pages = mm_virtual_early_alloc();
	size_t nrpages = mm_page_size(0) / sizeof(struct page);
	for(int i = 0; i < MAX_PGLEVEL + 1; i++) {
		_stacks[i].lock = SPINLOCK_INIT;
		_stacks[i].lock2 = SPINLOCK_INIT;
		_stacks[i].top = NULL;
		_stacks[i].avail = 0;
		_stacks[i].adding = false;
	}

	for(size_t i = 0; i < nrpages; i++, pages++) {
		pages->next = _stacks[0].top;
		pages->addr = mm_physical_early_alloc();
		pages->level = 0;
		pages->flags = PAGE_CACHE_WB;
		_stacks[0].top = pages;
		mm_page_bootstrap_count++;
		_stacks[0].avail++;
	}
}

void page_init(struct memregion *region)
{
	uintptr_t addr = region->start;
	if(addr == 0)
		addr += mm_page_size(0);
	printk("init region %lx -> %lx (%ld KB; %ld MB)\n",
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
		page->flags = PAGE_CACHE_WB;
		page->next = _stacks[level].top;
		page->parent = NULL;
		_stacks[level].top = page;
		_stacks[level].avail++;
		mm_page_count += mm_page_size(level) / mm_page_size(0);

		if(++i >= nrpages) {
			pages = mm_memory_alloc(mm_page_size(0), PM_TYPE_DRAM, true);
			i = 0;
			mm_page_alloc_count += mm_page_size(0);
		}

		addr += mm_page_size(level);
	}
	arena_create(&page_arena);
}

struct page *page_alloc(int flags, int level)
{
	if(!mm_ready)
		level = 0;
	spinlock_acquire_save(&_stacks[level].lock);
	struct page *p = _stacks[level].top;
	if(!p) {
		if(mm_ready) {
			panic("out of pages; level=%d", level);
		} else {
			page_init_bootstrap();
			return page_alloc(flags, level);
		}
	}
	_stacks[level].top = p->next;
	p->next = NULL;
	mm_page_alloced++;
	_stacks[level].avail--;

	if(_stacks[level].avail < 128 && level < MAX_PGLEVEL && mm_ready && !_stacks[level].adding) {
		_stacks[level].adding = true;
		spinlock_release_restore(&_stacks[level].lock);
		struct page *lp = page_alloc(flags, level + 1);
		printk("splitting page %lx\n", lp->addr);
		for(size_t i = 0; i < mm_page_size(level + 1) / mm_page_size(level); i++) {
			struct page *np = arena_allocate(&page_arena, sizeof(struct page));
			*np = *lp;
			np->addr += i * mm_page_size(level);
			np->parent = lp;
			np->level = level;
			spinlock_acquire_save(&_stacks[level].lock);
			np->next = _stacks[level].top;
			_stacks[level].top = np;
			_stacks[level].avail++;
			spinlock_release_restore(&_stacks[level].lock);
		}
		_stacks[level].adding = false;
		return page_alloc(flags, level);
	} else {
		spinlock_release_restore(&_stacks[level].lock);
	}

	return p;
}

struct slabcache sc_page;
struct slabcache sc_page_unalloc;

static void _page_unal_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct page *p = ptr;
	p->flags = 0;
	krc_init(&p->rc);
	p->lock = SPINLOCK_INIT;
	p->level = 0;
}

static void _page_ctor(void *_u, void *ptr)
{
	struct page *p = ptr;
	_page_unal_ctor(_u, ptr);
	p->type = PAGE_TYPE_VOLATILE; /* TODO */
	p->flags |= PAGE_ALLOCED;
}

static void _page_unal_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct page *p = ptr;
}

__initializer static void __init_page(void)
{
	slabcache_init(&sc_page, sizeof(struct page), _page_ctor, _page_unal_dtor, NULL);
	slabcache_init(&sc_page_unalloc, sizeof(struct page), _page_unal_ctor, _page_unal_dtor, NULL);
}

struct page *page_alloc_old(int type, int level)
{
	// if(type == PAGE_TYPE_VOLATILE) {
	//	struct page *p = slabcache_alloc(&sc_page_unalloc);
	//}
	struct page *p = slabcache_alloc(&sc_page_unalloc);
	p->type = type;
	p->level = level;
	// printk("allocating persistent memory\n");
	p->flags |= PAGE_ALLOCED;
	return p;
}

/* TODO: implement page pinning */
void page_pin(struct page *page)
{
	(void)page;
}

void page_unpin(struct page *page)
{
	(void)page;
}

struct page *page_alloc_nophys(void)
{
	return slabcache_alloc(&sc_page_unalloc);
}
