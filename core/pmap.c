#include <arena.h>
#include <memory.h>
#include <object.h>
#include <pmap.h>
#include <slots.h>
#include <spinlock.h>

/* TODO: put this in header */
extern struct vm_context kernel_ctx;

static _Atomic uintptr_t start = 0;

static struct object pmap_object;
static struct vmap pmap_vmap;

static struct arena page_arena;

static void pmap_init(void)
{
	obj_init(&pmap_object);
	pmap_object.kernel_obj = true;
	vm_vmap_init(&pmap_vmap, &pmap_object, KVSLOT_PMAP, VM_MAP_WRITE | VM_MAP_GLOBAL);
	vm_context_map(&kernel_ctx, &pmap_vmap);

	obj_alloc_slot(&pmap_object);
	arena_create(&page_arena);
}

void *pmap_allocate(uintptr_t phys, size_t len, int cache_type)
{
	static bool pmap_inited = false;
	if(!pmap_inited) {
		pmap_inited = true;
		pmap_init();
	}
	assert(len > 0);
	len = align_up(len, mm_page_size(0));
	uintptr_t s = atomic_fetch_add(&start, len);

	for(uintptr_t i = s; i < s + len; i += mm_page_size(0), phys += mm_page_size(0)) {
		struct page *page = arena_allocate(&page_arena, sizeof(struct page));
		page->flags = cache_type;
		page->addr = phys;
		page->level = 0;
		obj_cache_page(&pmap_object, s, page);
	}
	return (void *)(s + SLOT_TO_VADDR(KVSLOT_PMAP));
}
