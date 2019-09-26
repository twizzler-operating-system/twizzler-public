#include <memory.h>
#include <page.h>
#include <slab.h>

struct slabcache sc_page;
struct slabcache sc_page_unalloc;

static void _page_unal_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct page *p = ptr;
	p->flags = 0;
	krc_init(&p->rc);
	p->lock = SPINLOCK_INIT;
	p->mapcount = 0;
	p->level = 0;
}

static void _page_ctor(void *_u, void *ptr)
{
	struct page *p = ptr;
	_page_unal_ctor(_u, ptr);
	p->addr = mm_physical_alloc(mm_page_size(0), PM_TYPE_DRAM /* TODO */, true);
	p->type = PAGE_TYPE_VOLATILE; /* TODO */
	p->flags |= PAGE_ALLOCED;
}

static void _page_unal_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct page *p = ptr;
	if(p->addr && (p->flags & PAGE_ALLOCED))
		mm_physical_dealloc(p->addr);
}

__initializer static void __init_page(void)
{
	slabcache_init(&sc_page, sizeof(struct page), _page_ctor, _page_unal_dtor, NULL);
	slabcache_init(&sc_page_unalloc, sizeof(struct page), _page_unal_ctor, _page_unal_dtor, NULL);
}

struct page *page_alloc(int type)
{
	if(type == PAGE_TYPE_VOLATILE)
		return slabcache_alloc(&sc_page);
	struct page *p = slabcache_alloc(&sc_page_unalloc);
	p->type = type;
	p->addr = mm_physical_alloc(mm_page_size(0), PM_TYPE_NV, true);
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
