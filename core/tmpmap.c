#include <arena.h>
#include <memory.h>
#include <object.h>
#include <pmap.h>
#include <slots.h>
#include <spinlock.h>
#include <tmpmap.h>

/* TODO: put this in header */
extern struct vm_context kernel_ctx;

static struct spinlock lock = SPINLOCK_INIT;

static struct object tmpmap_object;
static struct vmap tmpmap_vmap;

static uint8_t *l0bitmap;
/* TODO: this is a waste of memory */
static uint8_t *l1bitmap;

__initializer static void tmpmap_init(void)
{
	obj_init(&tmpmap_object);
	tmpmap_object.kernel_obj = true;
	vm_vmap_init(&tmpmap_vmap, &tmpmap_object, KVSLOT_TMP_MAP, VM_MAP_WRITE | VM_MAP_GLOBAL);
	vm_context_map(&kernel_ctx, &tmpmap_vmap);

	obj_alloc_kernel_slot(&tmpmap_object);

	arch_vm_map_object(&kernel_ctx, &tmpmap_vmap, tmpmap_object.kslot);
	l0bitmap = mm_memory_alloc((OBJ_MAXSIZE / mm_page_size(0)) / 8, PM_TYPE_DRAM, true);
	l1bitmap = mm_memory_alloc((OBJ_MAXSIZE / mm_page_size(1)) / 8, PM_TYPE_DRAM, true);
}

void tmpmap_unmap_page(void *addr)
{
	uintptr_t virt = (uintptr_t)addr - (uintptr_t)SLOT_TO_VADDR(KVSLOT_TMP_MAP);
	int level = 0;
	if(virt >= OBJ_MAXSIZE / 2) {
		level = 1;
		virt -= OBJ_MAXSIZE / 2;
	}
	spinlock_acquire_save(&lock);
	uint8_t *bitmap = level ? l1bitmap : l0bitmap;
	size_t i = virt / mm_page_size(level);
	bitmap[i / 8] &= ~(1 << (i % 8));
	/* TODO: un-cache page from object */
	arch_object_unmap_page(&tmpmap_object, virt / mm_page_size(0));

	asm volatile("invlpg %0" ::"m"(addr) : "memory");

	spinlock_release_restore(&lock);
}

void *tmpmap_map_page(struct page *page)
{
	uintptr_t virt;
	spinlock_acquire_save(&lock);

	size_t max = (OBJ_MAXSIZE / mm_page_size(page->level)) / 2;
	uint8_t *bitmap = page->level ? l1bitmap : l0bitmap;
	size_t i;
	for(i = 0; i < max; i++) {
		if(!(bitmap[i / 8] & (1 << (i % 8)))) {
			bitmap[i / 8] |= (1 << (i % 8));
			break;
		}
	}
	spinlock_release_restore(&lock);
	if(i == max) {
		/* TODO: do something smarter */
		panic("out of tmp maps");
	}
	virt = i * mm_page_size(page->level);
	if(page->level)
		virt += OBJ_MAXSIZE / 2;

	obj_cache_page(&tmpmap_object, virt, page);
	// printk("tmpmap: %lx %lx\n", virt, SLOT_TO_VADDR(KVSLOT_TMP_MAP));
	return (void *)(virt + (uintptr_t)SLOT_TO_VADDR(KVSLOT_TMP_MAP));
}
