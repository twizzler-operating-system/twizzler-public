#include <lib/inthash.h>
#include <memory.h>
#include <object.h>
#include <processor.h>
#include <slab.h>

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

__initializer static void _init_vmctx(void)
{
	slabcache_init(&sc_iht, ihtable_size(8), _iht_ctor, NULL, (void *)8ul);
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

bool vm_map_contig(struct vm_context *v,
  uintptr_t virt,
  uintptr_t phys,
  size_t len,
  uintptr_t flags)
{
	for(int level = MAX_PGLEVEL; level >= 0; level--) {
		/* is this a valid level to map at? */
		size_t pgsz = mm_page_size(level);
		if(len % pgsz != 0) {
			continue;
		}

		for(size_t off = 0; off < len; off += pgsz) {
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
	struct vmap *m = ihtable_find(v->maps, slot, struct vmap, elem, slot);
	if(m) {
		panic("Map already exists");
	}

	struct object *obj = obj_lookup(objid);
	if(!obj) {
		return NULL;
	}
	m = slabcache_alloc(&sc_vmap);
	m->slot = slot;
	m->obj = obj; /* krc: move */
	m->flags = flags;
	m->status = 0;
	ihtable_insert(v->maps, &m->elem, m->slot);
	return m;
}

void kso_view_write(struct object *obj, size_t slot, struct viewentry *v)
{
	obj_write_data(obj, __VE_OFFSET + slot * sizeof(struct viewentry), sizeof(struct viewentry), v);
}

struct viewentry kso_view_lookup(struct vm_context *ctx, size_t slot)
{
	struct viewentry v;
	obj_read_data(kso_get_obj(ctx->view, view),
	  __VE_OFFSET + slot * sizeof(struct viewentry),
	  sizeof(struct viewentry),
	  &v);
	return v;
}

#include <twz/_thrd.h>
static bool lookup_by_slot(size_t slot, objid_t *id, uint64_t *flags)
{
	switch(slot) {
		struct viewentry ve;
		// case 0x10000:
		//	*id = kso_get_obj(current_thread->throbj, thr)->id;
		//	if(flags) *flags = VE_READ | VE_WRITE;
		// break;
		default:
			obj_read_data(kso_get_obj(current_thread->throbj, thr),
			  slot * sizeof(struct viewentry) + sizeof(struct twzthread_repr),
			  sizeof(struct viewentry),
			  &ve);
			if(ve.res0 == 0 && ve.res1 == 0 && ve.flags & VE_VALID) {
				//			printk("Slot %lx is fixed-point " IDFMT " %x\n", slot, IDPR(ve.id),
				// ve.flags);
				*id = ve.id;
				if(flags)
					*flags = ve.flags;
				return true;
			}
			ve = kso_view_lookup(current_thread->ctx, slot);
			//		printk("Slot %lx contains " IDFMT " %x\n", slot, IDPR(ve.id), ve.flags);
			if(ve.res0 != 0 || ve.res1 != 0 || !(ve.flags & VE_VALID)) {
				return false;
			}
			*id = ve.id;
			if(flags)
				*flags = ve.flags;
	}
	return true;
}

bool vm_vaddr_lookup(void *addr, objid_t *id, uint64_t *off)
{
	size_t slot = (uintptr_t)addr / mm_page_size(MAX_PGLEVEL);
	uint64_t o = (uintptr_t)addr % mm_page_size(MAX_PGLEVEL);

	if(off)
		*off = o;
	return lookup_by_slot(slot, id, NULL);
}

static bool _vm_view_invl(struct object *obj, struct kso_invl_args *invl)
{
	for(size_t slot = invl->offset / mm_page_size(MAX_PGLEVEL);
	    slot <= (invl->offset + invl->length) / mm_page_size(MAX_PGLEVEL);
	    slot++) {
		struct vmap *map = ihtable_find(current_thread->ctx->maps, slot, struct vmap, elem, slot);
		/* TODO (major): unmap all ctxs that use this view */
		if(map) {
			arch_vm_unmap_object(current_thread->ctx, map, obj);
			ihtable_remove(current_thread->ctx->maps, &map->elem, map->slot);
		}
	}
	return true;
}

bool vm_setview(struct thread *t, struct object *viewobj)
{
	obj_kso_init(viewobj, KSO_VIEW); // TODO
	// struct object *old = (t->ctx && t->ctx->view) ? kso_get_obj(t->ctx->view, view) : NULL;
	// struct vm_context *oldctx = t->ctx;
	t->ctx = vm_context_create();
	krc_get(&viewobj->refs);
	t->ctx->view = &viewobj->view;

	/* TODO: unmap things (or create a new context), destroy old, etc */
	/* TODO: check object type */
	return true;
}

static struct kso_calls _kso_view = {
	.ctor = NULL,
	.dtor = NULL,
	.attach = NULL,
	.detach = NULL,
	.invl = _vm_view_invl,
};

__initializer static void _init_kso_view(void)
{
	kso_register(KSO_VIEW, &_kso_view);
}

static inline void popul_info(struct fault_object_info *info,
  int flags,
  uintptr_t ip,
  uintptr_t addr,
  objid_t objid)
{
	memset(info, 0, sizeof(*info));
	if(!(flags & FAULT_ERROR_PERM)) {
		info->flags |= FAULT_OBJECT_NOMAP;
	}
	if(flags & FAULT_WRITE) {
		info->flags |= FAULT_OBJECT_WRITE;
	} else {
		info->flags |= FAULT_OBJECT_READ;
	}
	if(flags & FAULT_EXEC) {
		info->flags |= FAULT_OBJECT_EXEC;
	}
	info->ip = ip;
	info->addr = addr;
	info->objid = objid;
}

void vm_context_fault(uintptr_t ip, uintptr_t addr, int flags)
{
	// printk("Page Fault from %lx: %lx %x\n", ip, addr, flags);

	if(flags & FAULT_ERROR_PERM) {
		struct fault_object_info info;
		popul_info(&info, flags, ip, addr, 0);
		thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
		return;
	}
	size_t slot = addr / mm_page_size(MAX_PGLEVEL);
	struct vmap *map = ihtable_find(current_thread->ctx->maps, slot, struct vmap, elem, slot);
	if(!map) {
		objid_t id;
		uint64_t fl;
		if(!lookup_by_slot(slot, &id, &fl)) {
			struct fault_object_info info;
			popul_info(&info, flags, ip, addr, 0);
			thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
			return;
		}
		map = vm_context_map(current_thread->ctx, id, slot, fl & (VE_READ | VE_WRITE | VE_EXEC));
		if(!map) {
			struct fault_object_info info;
			popul_info(&info, flags, ip, addr, id);
			info.flags |= FAULT_OBJECT_EXIST;
			thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
			return;
		}
	}
	if(map->obj->slot == -1) {
		obj_alloc_slot(map->obj);
	}
	arch_vm_map_object(current_thread->ctx, map, map->obj);
}
