#include <memory.h>
#include <slab.h>
#include <lib/inthash.h>
#include <processor.h>
#include <object.h>

struct slabcache sc_vmctx, sc_iht, sc_vmap;

static void _vmctx_ctor(void *_p, void *obj)
{
	(void)_p;
	struct vm_context *v = obj;
	v->maps = slabcache_alloc(&sc_iht);
	arch_mm_context_init(v);
}

static void _vmctx_dtor(void *_p, void *obj)
{
	(void)_p;
	struct vm_context *v = obj;
	slabcache_free(v->maps);
}

__initializer
static void _init_vmctx(void)
{
	slabcache_init(&sc_iht, ihtable_size(4), _iht_ctor, NULL, (void *)4ul);
	slabcache_init(&sc_vmap, sizeof(struct vmap), NULL, NULL, NULL);
	slabcache_init(&sc_vmctx, sizeof(struct vm_context), _vmctx_ctor, _vmctx_dtor, NULL);
}

struct vm_context *vm_context_create(void)
{
	return slabcache_alloc(&sc_vmctx);
}

void vm_context_destroy(struct vm_context *v)
{
	/* TODO: unmap things? */
	slabcache_free(v);
}

int vm_context_map(struct vm_context *v, uint128_t objid, size_t slot, uint32_t flags)
{
	ihtable_lock(v->maps);
	struct vmap *m = ihtable_find(v->maps, slot, struct vmap, elem, slot);
	if(m) {
		ihtable_unlock(v->maps);
		return -1;
	}
	m = slabcache_alloc(&sc_vmap);
	m->slot = slot;
	m->target = objid;
	m->flags = flags;
	m->status = 0;
	ihtable_insert(v->maps, &m->elem, m->slot);
	ihtable_unlock(v->maps);
	return 0;
}

void vm_context_fault(uintptr_t addr, int flags)
{
	printk("Page Fault: %lx %x\n", addr, flags);
	if(flags & FAULT_ERROR_PERM) {
		/* TODO: COW here? */
		panic("page fault addr=%lx flags=%x\n", addr, flags);
	}
	size_t slot = addr / mm_page_size(MAX_PGLEVEL);
	struct vmap *map = ihtable_find(current_thread->ctx->maps, slot, struct vmap, elem, slot);
	printk("mapping virt slot %ld -> obj " PR128FMTd "\n", map->slot, PR128(map->target));

	struct object *obj = obj_lookup(map->target);
	if(obj == NULL) {
		panic("object " PR128FMTd " not found", PR128(map->target));
	}
	if(obj->slot == -1) {
		obj_alloc_slot(obj);
	}
	arch_vm_map_object(current_thread->ctx, map, obj);
}

