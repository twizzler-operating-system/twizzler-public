#include <slab.h>
#include <memory.h>
static struct slab *alloc_slab(struct slab_allocator *alloc)
{
	size_t slab_size = sizeof(struct slab) + sizeof(struct slab_object) * alloc->num_per_slab + alloc->size * alloc->num_per_slab;
	struct slab *slab = (void *)mm_virtual_alloc(slab_size, PM_TYPE_ANY, true);
	slab->alloc = alloc;
	slab->lock = SPINLOCK_INIT;
	linkedlist_create(&slab->list, LINKEDLIST_LOCKLESS);
	for(size_t i = 0;i<alloc->num_per_slab;i++) {
		struct slab_object *so = &slab->objects[i];
		void *obj = (char *)(&slab->objects[alloc->num_per_slab]) + alloc->size * i;
		so->obj = obj;
		linkedlist_insert(&slab->list, &so->entry, so);
	}
	return slab;
}

#if 0
static void destroy_slab(struct slab_allocator *alloc, struct slab *slab)
{
	(void)alloc;
	(void)slab;
	panic("not implemented");
}
#endif

static struct slab_object *alloc_object_from_slab(struct slab_allocator *alloc, struct slab *slab)
{
	spinlock_acquire(&slab->lock);
	assert(slab->count < alloc->num_per_slab);

	struct slab_object *obj = linkedlist_remove_head(&slab->list);
	slab->count++;

	spinlock_release(&slab->lock);
	return obj;
}

void *slab_alloc(struct slab_allocator *alloc)
{
	spinlock_acquire(&alloc->lock);
	if(!alloc->initialized) {
		alloc->initialized = true;
		linkedlist_create(&alloc->full,    LINKEDLIST_LOCKLESS);
		linkedlist_create(&alloc->partial, LINKEDLIST_LOCKLESS);
		linkedlist_create(&alloc->empty,   LINKEDLIST_LOCKLESS);
	}
	
	struct slab *slab;
	if(alloc->partial.count > 0) {
		slab = linkedlist_head(&alloc->partial);
		if(slab->count == alloc->num_per_slab - 1) {
			linkedlist_remove(&alloc->partial, &slab->entry);
			linkedlist_insert(&alloc->full, &slab->entry, slab);
		}
	} else if(alloc->empty.count > 0) {
		slab = linkedlist_remove_head(&alloc->empty);
		linkedlist_insert(&alloc->partial, &slab->entry, slab);
	} else {
		slab = alloc_slab(alloc);
		linkedlist_insert(&alloc->partial, &slab->entry, slab);
	}

	struct slab_object *obj = alloc_object_from_slab(alloc, slab);
	struct slab **ret = obj->obj;
	*ret = slab;
	ret++;

	spinlock_release(&alloc->lock);
	assert(!(obj->flags & SLAB_OBJ_ALLOCATED));
	if(obj->flags & SLAB_OBJ_CREATED) {
		if(alloc->init) {
			alloc->init(ret);
		}
	} else {
		obj->flags |= SLAB_OBJ_CREATED;
		if(alloc->create) {
			alloc->create(ret);
		}
	}
	obj->flags |= SLAB_OBJ_ALLOCATED;
	return ret;
}

void slab_release(void *obj)
{
	assert(obj != NULL);
	struct slab **slab_ptr = obj;
	slab_ptr--;

	struct slab *slab = *slab_ptr;
	struct slab_allocator *alloc = slab->alloc;

	size_t objnr = (size_t)((char *)obj - (char *)(&slab->objects[alloc->num_per_slab])) / alloc->size;
	struct slab_object *so = &slab->objects[objnr];

	assert(so->flags & SLAB_OBJ_ALLOCATED);
	assert(so->flags & SLAB_OBJ_CREATED);
	so->flags &= ~SLAB_OBJ_ALLOCATED;
	if(alloc->release)
		alloc->release(obj);

	spinlock_acquire(&alloc->lock);
	spinlock_acquire(&slab->lock);
	slab->count--;
	linkedlist_insert(&slab->list, &so->entry, so);
	spinlock_release(&slab->lock);

	if(slab->count == alloc->num_per_slab - 1) {
		linkedlist_remove(&alloc->full, &slab->entry);
		linkedlist_insert(&alloc->partial, &slab->entry, slab);
	} else if(slab->count == 0) {
		linkedlist_remove(&alloc->partial, &slab->entry);
		linkedlist_insert(&alloc->empty, &slab->entry, slab);
	}

	spinlock_release(&alloc->lock);
}

