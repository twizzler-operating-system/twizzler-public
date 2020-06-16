#include <lib/inthash.h>
#include <memory.h>
#include <object.h>
#include <pager.h>
#include <processor.h>
#include <slab.h>
#include <slots.h>

struct vm_context kernel_ctx = {};

static struct slabcache sc_vmctx, sc_vmap;

static DECLARE_LIST(kvmap_stack);
static struct spinlock kvmap_lock = SPINLOCK_INIT;

size_t vm_max_slot(void)
{
	static size_t _max_vslot = 0;
	if(!_max_vslot)
		_max_vslot = (2ul << arch_processor_virtual_width()) / OBJ_MAXSIZE;
	return _max_vslot - 1;
}

static void _vmctx_ctor(void *_p, void *obj)
{
	(void)_p;
	struct vm_context *v = obj;
	arch_mm_context_init(v);
}

static void _vmctx_dtor(void *_p, void *obj)
{
	(void)_p;
	(void)obj;
}

__initializer static void _init_vmctx(void)
{
	slabcache_init(&sc_vmap, "sc_vmap", sizeof(struct vmap), NULL, NULL, NULL);
	slabcache_init(
	  &sc_vmctx, "sc_vmctx", sizeof(struct vm_context), _vmctx_ctor, _vmctx_dtor, NULL);
}

struct vm_context *vm_context_create(void)
{
	struct vm_context *ctx = slabcache_alloc(&sc_vmctx);
	krc_init(&ctx->refs);
	return ctx;
}

static void vm_map_disestablish(struct vm_context *ctx, struct vmap *map)
{
	arch_vm_unmap_object(ctx, map);
	struct object *obj = map->obj;
	map->obj = NULL;
	rb_delete(&map->node, &ctx->root);

	obj_release_slot(obj);
	obj_put(obj);

	slabcache_free(&sc_vmap, map);
}

static void __vm_context_finish_destroy(void *_v)
{
	struct vm_context *v = _v;
	arch_mm_context_destroy(v);
	slabcache_free(&sc_vmctx, v);
}

void vm_context_destroy(struct vm_context *v)
{
	/* TODO (major): unmap things? */
	struct rbnode *next;
	for(struct rbnode *node = rb_first(&v->root); node; node = next) {
		struct vmap *map = rb_entry(node, struct vmap, node);
#if CONFIG_DEBUG_OBJECT_SLOT
		printk("unmap: " IDFMT ": %ld, with map count %ld\n",
		  IDPR(map->obj->id),
		  map->slot,
		  map->obj->mapcount.count);
#endif

		next = rb_next(node);
		vm_map_disestablish(v, map);
	}

	/* TODO: does this work for all contexts? */
	struct object *obj = kso_get_obj(v->view, view);
#if CONFIG_DEBUG_OBJECT_SLOT
	printk("VM_CONTEXT_DESTROY OBJ: " IDFMT "\n", IDPR(obj->id));
#endif
	obj_free_kaddr(obj);
	obj_put(obj); /* one for kso, one for this ref. TODO: clean this up */
	obj_put(obj);

	workqueue_insert(&current_processor->wq, &v->free_task, __vm_context_finish_destroy, v);
}

void vm_context_put(struct vm_context *v)
{
	if(krc_put(&v->refs)) {
		vm_context_destroy(v);
	}
}

static int vmap_compar_key(struct vmap *v, size_t slot)
{
	if(v->slot > slot)
		return 1;
	if(v->slot < slot)
		return -1;
	return 0;
}

static int vmap_compar(struct vmap *a, struct vmap *b)
{
	return vmap_compar_key(a, b->slot);
}

void vm_vmap_init(struct vmap *vmap, struct object *obj, size_t vslot, uint32_t flags)
{
	vmap->slot = vslot;
	krc_get(&obj->refs);
	vmap->obj = obj;
	vmap->flags = flags;
	vmap->status = 0;
}

static void vm_map_establish(struct vmap *map)
{
	struct slot *slot = obj_alloc_slot(map->obj);
	if(slot) {
		arch_vm_map_object(current_thread->ctx, map, slot);
		slot_release(slot);
	}
}

void vm_context_map(struct vm_context *v, struct vmap *m)
{
	if(!rb_insert(&v->root, m, struct vmap, node, vmap_compar)) {
		panic("Map already exists");
	}
}

void kso_view_write(struct object *obj, size_t slot, struct viewentry *v)
{
	obj_write_data(obj, __VE_OFFSET + slot * sizeof(struct viewentry), sizeof(struct viewentry), v);
}

static struct viewentry kso_view_lookup(struct vm_context *ctx, size_t slot)
{
	struct viewentry v;
	/* TODO: make sure these reads aren't too costly. These are to KSO objects, so they should be
	 * okay. */
	struct object *obj = kso_get_obj(ctx->view, view);
	obj_read_data(obj, __VE_OFFSET + slot * sizeof(struct viewentry), sizeof(struct viewentry), &v);
	obj_put(obj);
	return v;
}

#include <twz/_thrd.h>
static bool lookup_by_slot(size_t slot, objid_t *id, uint64_t *flags)
{
	switch(slot) {
		struct object *obj;
		struct viewentry ve;
		default:
			obj = kso_get_obj(current_thread->throbj, thr);
			obj_read_data(obj,
			  slot * sizeof(struct viewentry) + sizeof(struct twzthread_repr),
			  sizeof(struct viewentry),
			  &ve);
			obj_put(obj);
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
	(void)obj;
	spinlock_acquire_save(&current_thread->ctx->lock);
	for(size_t slot = invl->offset / mm_page_size(MAX_PGLEVEL);
	    slot <= (invl->offset + invl->length) / mm_page_size(MAX_PGLEVEL);
	    slot++) {
		struct rbnode *node =
		  rb_search(&current_thread->ctx->root, slot, struct vmap, node, vmap_compar_key);
		/* TODO (major): unmap all ctxs that use this view */
		if(node) {
			struct vmap *map = rb_entry(node, struct vmap, node);
#if CONFIG_DEBUG_OBJECT_SLOT
			printk("UNMAP VIA INVAL: " IDFMT " mapcount %ld\n",
			  IDPR(map->obj->id),
			  map->obj->mapcount.count);
#endif
			//			printk("B :: " IDFMT "\n", IDPR(map->obj->id));
			vm_map_disestablish(current_thread->ctx, map);
			// arch_vm_unmap_object(current_thread->ctx, map);
			// rb_delete(node, &current_thread->ctx->root);
		}
	}
	spinlock_release_restore(&current_thread->ctx->lock);
	return true;
}

bool vm_setview(struct thread *t, struct object *viewobj)
{
	obj_kso_init(viewobj, KSO_VIEW); // TODO
	struct vm_context *oldctx = t->ctx;

	t->ctx = vm_context_create();
	krc_get(&viewobj->refs);
	t->ctx->view = &viewobj->view;

	if(oldctx)
		vm_context_destroy(oldctx);
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
	uint64_t iflags = 0;
	if(!(flags & FAULT_ERROR_PERM)) {
		iflags |= FAULT_OBJECT_NOMAP;
	}
	if(flags & FAULT_WRITE) {
		iflags |= FAULT_OBJECT_WRITE;
	} else {
		iflags |= FAULT_OBJECT_READ;
	}
	if(flags & FAULT_EXEC) {
		iflags |= FAULT_OBJECT_EXEC;
	}
	*info = twz_fault_build_object_info(objid, (void *)ip, (void *)addr, iflags);
}

static void vm_kernel_alloc_slot(struct object *obj)
{
	spinlock_acquire_save(&kvmap_lock);
	static ssize_t counter = -1;
	if(counter == -1)
		counter = KVSLOT_START;

	struct vmap *m;
	if(list_empty(&kvmap_stack)) {
		m = slabcache_alloc(&sc_vmap);
		if((size_t)counter == KVSLOT_MAX) {
			panic("out of kvslots");
		}
		m->slot = counter++;
	} else {
		struct list *e = list_pop(&kvmap_stack);
		m = list_entry(e, struct vmap, entry);
	}

	vm_vmap_init(m, obj, m->slot, VE_READ | VE_WRITE);
	spinlock_release_restore(&kvmap_lock);

	obj->kvmap = m;
}

void vm_kernel_map_object(struct object *obj)
{
	assert(obj->kslot);
	assert(!obj->kvmap);

	vm_kernel_alloc_slot(obj);
	spinlock_acquire_save(&kernel_ctx.lock);
	vm_context_map(&kernel_ctx, obj->kvmap);
	arch_vm_map_object(&kernel_ctx, obj->kvmap, obj->kslot);
	spinlock_release_restore(&kernel_ctx.lock);
}

void vm_kernel_unmap_object(struct object *obj)
{
	struct vmap *vmap = obj->kvmap;
	obj->kvmap = NULL;

	spinlock_acquire_save(&kernel_ctx.lock);
	arch_vm_unmap_object(&kernel_ctx, vmap);
	struct object *xobj = vmap->obj;
	vmap->obj = NULL;
	rb_delete(&vmap->node, &kernel_ctx.root);
	spinlock_release_restore(&kernel_ctx.lock);

	spinlock_acquire_save(&kvmap_lock);

	list_insert(&kvmap_stack, &vmap->entry);

	spinlock_release_restore(&kvmap_lock);

	obj_put(xobj);
}

#include <queue.h>
static int __do_map(struct vm_context *ctx,
  uintptr_t ip,
  uintptr_t addr,
  int flags,
  bool fault,
  bool wire)
{
	size_t slot = addr / mm_page_size(MAX_PGLEVEL);
	if(slot >= KVSLOT_START)
		return -EPERM;
	struct vmap *map = NULL;
	spinlock_acquire_save(&ctx->lock);
	struct rbnode *node = rb_search(&ctx->root, slot, struct vmap, node, vmap_compar_key);
	if(node) {
		map = rb_entry(node, struct vmap, node);
	}
	if(!map) {
		objid_t id;
		uint64_t fl;
		if(!lookup_by_slot(slot, &id, &fl)) {
			spinlock_release_restore(&ctx->lock);
			if(fault) {
				struct fault_object_info info;
				popul_info(&info, flags, ip, addr, 0);
				thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
			}
			return -EINVAL;
		}
		struct object *obj = obj_lookup(id, 0);
		if(!obj) {
			spinlock_release_restore(&ctx->lock);
			if(fault) {
				if(kernel_queue_pager_request_object(id)) {
					struct fault_object_info info;
					popul_info(&info, flags, ip, addr, id);
					info.flags |= FAULT_OBJECT_EXIST;
					thread_raise_fault(current_thread, FAULT_OBJECT, &info, sizeof(info));
				}
				return 0;
			}
			return -ENOENT;
		}
		map = slabcache_alloc(&sc_vmap);
		vm_vmap_init(map, obj, slot, fl & (VE_READ | VE_WRITE | VE_EXEC));
		obj_put(obj);
		vm_context_map(ctx, map);
	}
	if(wire) {
		map->status |= VMAP_WIRE;
	}
	vm_map_establish(map);
	spinlock_release_restore(&ctx->lock);
	return 0;
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
	__do_map(current_thread->ctx, ip, addr, flags, true, false);
	// printk("done pf\n");
}

int vm_context_wire(const void *p)
{
	return __do_map(
	  current_thread->ctx, arch_thread_instruction_pointer(), (uintptr_t)p, 0, false, true);
}
