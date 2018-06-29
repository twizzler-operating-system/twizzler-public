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
	/* TODO (major): unmap things? */
	slabcache_free(v);
}

bool vm_map_contig(struct vm_context *v, uintptr_t virt, uintptr_t phys, size_t len, uintptr_t flags)
{
	for(int level=MAX_PGLEVEL;level >= 0;level--) {
		/* is this a valid level to map at? */
		size_t pgsz = mm_page_size(level);
		if(len % pgsz != 0) {
			continue;
		}

		for(size_t off=0;off < len;off += pgsz) {
			if(!arch_vm_map(v, virt + off, phys + off, level, flags)) {
				panic("overwriting existing mapping at %lx", virt + off);
			}
		}
		return true;
	}
	panic("unable to map region (len=%ld) at any level", len);
}

struct vmap *vm_context_map(struct vm_context *v, uint128_t objid, size_t slot, uint32_t flags)
{
	ihtable_lock(v->maps);
	struct vmap *m = ihtable_find(v->maps, slot, struct vmap, elem, slot);
	if(m) {
		ihtable_unlock(v->maps);
		return NULL;
	}
	m = slabcache_alloc(&sc_vmap);
	m->slot = slot;
	m->target = objid;
	m->flags = flags;
	m->status = 0;
	ihtable_insert(v->maps, &m->elem, m->slot);
	ihtable_unlock(v->maps);
	return m;
}

struct viewentry kso_view_lookup(struct vm_context *ctx, size_t slot)
{
	struct viewentry v;
	obj_read_data(kso_get_obj(ctx->view, view), slot * sizeof(struct viewentry),
			sizeof(struct viewentry), &v);
	return v;
}

static bool lookup_by_slot(size_t slot, objid_t *id, uint64_t *flags)
{
	switch(slot) {
		struct viewentry ve;
		case 0x10000:
			*id = kso_get_obj(current_thread->throbj, thr)->id;
			if(flags) *flags = VE_READ | VE_WRITE;
		break;
		default:
			ve = kso_view_lookup(current_thread->ctx, slot);
			if(ve.res0 != 0 || ve.res1 != 0 || !(ve.flags & VE_VALID)) {
				return false;
			}
			*id = ve.id;
			if(flags) *flags = ve.flags;
	}
	return true;
}

bool vm_vaddr_lookup(void *addr, objid_t *id, uint64_t *off)
{
	size_t slot = (uintptr_t)addr / mm_page_size(MAX_PGLEVEL);
	uint64_t o = (uintptr_t)addr % mm_page_size(MAX_PGLEVEL);

	*off = o;
	return lookup_by_slot(slot, id, NULL);
}

static bool _vm_view_invl(struct object *obj, struct kso_invl_args *invl)
{
	for(size_t slot = invl->offset / mm_page_size(MAX_PGLEVEL);
			slot <= (invl->offset + invl->length);
			slot++) {
		struct vmap *map = ihtable_find(current_thread->ctx->maps, slot, struct vmap, elem, slot);
		/* TODO (major): unmap all ctxs that use this view */
		arch_vm_unmap_object(current_thread->ctx, map, obj);
		ihtable_remove(current_thread->ctx->maps, &map->elem, map->slot);
	}
	return true;
}

bool vm_setview(struct thread *t, struct object *viewobj)
{
	obj_kso_init(viewobj, KSO_VIEW); //TODO
	struct object *old = (t && t->ctx->view) ? 
		kso_get_obj(t->ctx->view, view) : NULL;
	struct vm_context *oldctx = t->ctx;
	t->ctx = vm_context_create();
	t->ctx->view = &viewobj->view;
	/* TODO: unmap things (or create a new context), destroy old, etc */
	/* TODO: check object type */
	return true;
}

static struct kso_calls _kso_view = {
	.ctor   = NULL,
	.dtor   = NULL,
	.attach = NULL,
	.detach = NULL,
	.invl   = _vm_view_invl,
};

__initializer static void _init_kso_view(void)
{
	kso_register(KSO_VIEW, &_kso_view);
}

void vm_context_fault(uintptr_t addr, int flags)
{
	printk("Page Fault: %lx %x\n", addr, flags);
	if(flags & FAULT_ERROR_PERM) {
		/* TODO (major): COW here? */
		panic("page fault addr=%lx flags=%x\n", addr, flags);
	}
	size_t slot = addr / mm_page_size(MAX_PGLEVEL);
	struct vmap *map = ihtable_find(current_thread->ctx->maps, slot, struct vmap, elem, slot);
	if(!map) {
		objid_t id;
		uint64_t flags;
		if(!lookup_by_slot(slot, &id, &flags)) {
			panic("raise signal");
		}
		map = vm_context_map(current_thread->ctx, id, slot,
				flags & (VE_READ | VE_WRITE | VE_EXEC));
	}
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

