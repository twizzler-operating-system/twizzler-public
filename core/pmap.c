#include <arena.h>
#include <memory.h>
#include <object.h>
#include <pmap.h>
#include <slots.h>
#include <spinlock.h>

/* TODO: put this in header */
extern struct vm_context kernel_ctx;

struct pmap {
	uintptr_t phys;
	uintptr_t virt;
	struct rbnode node;
	struct page page;
};

static struct spinlock lock = SPINLOCK_INIT;

static uintptr_t start = 0;

static struct object pmap_object;
static struct vmap pmap_vmap;

static struct arena pmap_arena;

static struct rbroot root = RBINIT;

static int __pmap_compar_key(struct pmap *a, uintptr_t n)
{
	if((a->phys | a->page.flags) > n)
		return 1;
	else if((a->phys | a->page.flags) < n)
		return -1;
	return 0;
}

static int __pmap_compar(struct pmap *a, struct pmap *b)
{
	assert((b->phys & b->page.flags) == 0);
	return __pmap_compar_key(a, b->phys | b->page.flags);
}

static void pmap_init(void)
{
	obj_init(&pmap_object);
	pmap_object.flags = OF_KERNEL;
	vm_vmap_init(&pmap_vmap, &pmap_object, KVSLOT_PMAP, VM_MAP_WRITE | VM_MAP_GLOBAL);
	vm_context_map(&kernel_ctx, &pmap_vmap);

	obj_alloc_kernel_slot(&pmap_object);
	arena_create(&pmap_arena);

	arch_vm_map_object(&kernel_ctx, &pmap_vmap, pmap_object.kslot);
}

static struct pmap *pmap_get(uintptr_t phys, int cache_type, bool remap)
{
	struct pmap *pmap;
	struct rbnode *node = rb_search(&root, phys, struct pmap, node, __pmap_compar_key);
	if(!node) {
		pmap = arena_allocate(&pmap_arena, sizeof(struct pmap));
		pmap->phys = phys;
		pmap->virt = start;
		start += mm_page_size(0);
		pmap->page.flags = cache_type;
		pmap->page.addr = phys;
		pmap->page.level = 0;
		rb_insert(&root, pmap, struct pmap, node, __pmap_compar);

		obj_cache_page(&pmap_object, pmap->virt, &pmap->page);

	} else {
		pmap = rb_entry(node, struct pmap, node);
		if(remap) {
			obj_cache_page(&pmap_object, start, &pmap->page);
			start += mm_page_size(0);
		}
	}

	return pmap;
}

void *pmap_allocate(uintptr_t phys, size_t len, int cache_type)
{
	static bool pmap_inited = false;
	if(!pmap_inited) {
		pmap_inited = true;
		pmap_init();
	}
	size_t off = phys % mm_page_size(0);
	len += phys % mm_page_size(0);
	phys = align_down(phys, mm_page_size(0));

	spinlock_acquire_save(&lock);
	uintptr_t virt = 0;
	for(size_t i = 0; i < len; i += mm_page_size(0)) {
		struct pmap *pmap = pmap_get(phys, cache_type, len > mm_page_size(0));
		assert(pmap != NULL);
		if(!virt)
			virt = pmap->virt;
	}
	spinlock_release_restore(&lock);
	// printk("pmap alloc %lx:%lx -> %lx\n", phys, phys + len - 1, virt + off);
	return (void *)(virt + off + (uintptr_t)SLOT_TO_VADDR(KVSLOT_PMAP));
}
