#include <object.h>
#include <page.h>
#include <slots.h>

#if 0
void obj_release_kernel_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(!obj->kslot) {
		spinlock_release_restore(&obj->lock);
		return;
	}

	struct slot *slot = obj->kslot;
	obj->kslot = NULL;
	spinlock_release_restore(&obj->lock);

	vm_kernel_unmap_object(obj);
	object_space_release_slot(slot);
	slot_release(slot);
}
#endif

static void __obj_alloc_kernel_slot(struct object *obj)
{
	if(obj->kslot)
		return;
	struct slot *slot = slot_alloc();
#if CONFIG_DEBUG_OBJECT_SLOT
	printk("[slot]: allocated kslot %ld for " IDFMT " (%p %lx %d)\n",
	  slot->num,
	  IDPR(obj->id),
	  obj,
	  obj->flags,
	  obj->kso_type);
#endif
	krc_get(&obj->refs);
	slot->obj = obj;   /* above get */
	obj->kslot = slot; /* move ref */
}

void obj_alloc_kernel_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	__obj_alloc_kernel_slot(obj);
	spinlock_release_restore(&obj->lock);
}

void *obj_get_kaddr(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);

	if(obj->kaddr == NULL) {
		assert(krc_iszero(&obj->kaddr_count));
		__obj_alloc_kernel_slot(obj);
		if(!obj->kvmap) {
			vm_kernel_map_object(obj);
		}
		obj->kaddr = (char *)SLOT_TO_VADDR(obj->kvmap->slot);
	}

	krc_get(&obj->kaddr_count);

	spinlock_release_restore(&obj->lock);

	return obj->kaddr;
}

void obj_free_kaddr(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(obj->kaddr == NULL) {
		spinlock_release_restore(&obj->lock);
		return;
	}
	krc_init_zero(&obj->kaddr_count);
	struct slot *slot = obj->kslot;
	obj->kslot = NULL;
	obj->kaddr = NULL;

	vm_kernel_unmap_object(obj);
	spinlock_release_restore(&obj->lock);
	struct object *o = slot->obj;
	assert(o == obj);
	slot->obj = NULL;
	object_space_release_slot(slot);
	slot_release(slot);
	obj_put(o);
}

bool obj_kaddr_valid(struct object *obj, void *kaddr, size_t run)
{
	void *ka = obj_get_kaddr(obj);
	bool ok = (kaddr >= ka && ((char *)kaddr + run) < (char *)ka + OBJ_MAXSIZE);
	obj_release_kaddr(obj);
	return ok;
}

void obj_release_kaddr(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(obj->kaddr == NULL) {
		spinlock_release_restore(&obj->lock);
		return;
	}
	if(krc_put(&obj->kaddr_count)) {
		/* TODO: but make these reclaimable */
		if(obj->kso_type)
			goto done;
		struct slot *slot = obj->kslot;
		obj->kslot = NULL;
		obj->kaddr = NULL;

		vm_kernel_unmap_object(obj);
		spinlock_release_restore(&obj->lock);
		struct object *o = slot->obj;
		assert(o == obj);
		slot->obj = NULL;
		obj_put(o);
		object_space_release_slot(slot);
		slot_release(slot);
	}
done:
	spinlock_release_restore(&obj->lock);
}

/* TODO (major): these can probably fail */
void obj_read_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);

#if 0

	// printk(":: reading %lx -> %lx\n", start, start + len);
	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);
#endif
	void *addr = (char *)obj_get_kaddr(obj) + start;
	// void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + start;
	memcpy(ptr, addr, len);
	obj_release_kaddr(obj);
}

void obj_write_data(struct object *obj, size_t start, size_t len, void *ptr)
{
	start += OBJ_NULLPAGE_SIZE;
	assert(start < OBJ_MAXSIZE && start + len <= OBJ_MAXSIZE && len < OBJ_MAXSIZE);

#if 0
	obj_alloc_kernel_slot(obj);
	if(!obj->kvmap)
		vm_kernel_map_object(obj);
#endif

	void *addr = (char *)obj_get_kaddr(obj) + start;

	// void *addr = (char *)SLOT_TO_VADDR(obj->kvmap->slot) + start;
	memcpy(addr, ptr, len);
	obj_release_kaddr(obj);
}

void obj_write_data_atomic64(struct object *obj, size_t off, uint64_t val)
{
	off += OBJ_NULLPAGE_SIZE;
	assert(off < OBJ_MAXSIZE && off + 8 <= OBJ_MAXSIZE);

	void *addr = (char *)obj_get_kaddr(obj) + off;
	*(_Atomic uint64_t *)addr = val;
	obj_release_kaddr(obj);
}
