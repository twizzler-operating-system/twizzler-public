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

static int __pmap_compar_key(struct pmap *a, size_t n)
{
	if(a->phys > n)
		return 1;
	else if(a->phys < n)
		return -1;
	return 0;
}

static int __pmap_compar(struct pmap *a, struct pmap *b)
{
	return __pmap_compar_key(a, b->phys);
}

static void pmap_init(void)
{
	obj_init(&pmap_object);
	pmap_object.kernel_obj = true;
	vm_vmap_init(&pmap_vmap, &pmap_object, KVSLOT_PMAP, VM_MAP_WRITE | VM_MAP_GLOBAL);
	vm_context_map(&kernel_ctx, &pmap_vmap);

	obj_alloc_slot(&pmap_object);
	arena_create(&pmap_arena);
	printk("pmap init %ld\n", pmap_object.slot->num);

	arch_vm_map_object(&kernel_ctx, &pmap_vmap, &pmap_object);
}

static struct pmap *pmap_get(uintptr_t phys, int cache_type)
{
	struct pmap *pmap;
	struct rbnode *node = rb_search(&root, phys, struct pmap, node, __pmap_compar_key);
	if(!node) {
		pmap = arena_allocate(&pmap_arena, sizeof(struct pmap));
		pmap->phys = phys;
		rb_insert(&root, pmap, struct pmap, node, __pmap_compar);

		pmap->virt = start;
		start += mm_page_size(0);
		pmap->page.flags = cache_type;
		pmap->page.addr = phys;
		pmap->page.level = 0;

		obj_cache_page(&pmap_object, pmap->virt, &pmap->page);

	} else {
		pmap = rb_entry(node, struct pmap, node);
	}

	return pmap;
}

void *pmap_allocate(uintptr_t phys, int cache_type)
{
	static bool pmap_inited = false;
	if(!pmap_inited) {
		pmap_inited = true;
		pmap_init();
	}
	size_t off = phys % mm_page_size(0);
	phys = align_down(phys, mm_page_size(0));

	spinlock_acquire_save(&lock);
	struct pmap *pmap = pmap_get(phys, cache_type);
	assert(pmap != NULL);
	spinlock_release_restore(&lock);
	return (void *)(pmap->virt + off + (uintptr_t)SLOT_TO_VADDR(KVSLOT_PMAP));
}
