#include <slab.h>

static struct slab *alloc_slab(struct slab_allocator *alloc)
{

}

static void destroy_slab(struct slab_allocator *alloc, struct slab *slab)
{

}

static struct slab_object *alloc_object_from_slab(struct slab_allocator *alloc, struct slab *slab)
{

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
	struct slab **ret = linkedentry_obj(&obj->entry);
	*ret = slab;
	ret++;

	spinlock_release(&alloc->lock);

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
	return ret;
}

void slab_release(struct slab_allocator *alloc, void *obj)
{

}

