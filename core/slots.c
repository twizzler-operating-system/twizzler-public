#include <lib/iter.h>
#include <lib/rb.h>
#include <object.h>
#include <processor.h>
#include <secctx.h>
#include <slab.h>
#include <slots.h>
#include <spinlock.h>

#include <twz/_obj.h>

struct slot_entry {
	struct object_space *space;
	struct list entry;
};

static DECLARE_SLABCACHE(_sc_slot_entry, sizeof(struct slot_entry), NULL, NULL, NULL);
static DECLARE_SLABCACHE(_sc_slot, sizeof(struct slot), NULL, NULL, NULL);

static struct rbroot slot_root = RBINIT;

static struct slot *slot_stack = NULL;
static struct spinlock slot_lock = SPINLOCK_INIT;

static size_t _max_slot = 0;
static size_t skip_bootstrap_slot;

static struct object _bootstrap_object;
static struct slot _bootstrap_slot;
static struct vmap _bootstrap_vmap;

static int __slot_compar_key(struct slot *a, size_t n)
{
	if(a->num > n)
		return 1;
	else if(a->num < n)
		return -1;
	return 0;
}

static int __slot_compar(struct slot *a, struct slot *b)
{
	return __slot_compar_key(a, b->num);
}

void slot_init_bootstrap(size_t oslot, size_t vslot)
{
	_bootstrap_vmap.obj = &_bootstrap_object;
	_bootstrap_vmap.slot = vslot;
	obj_init(&_bootstrap_object);
	_bootstrap_slot.num = oslot;
	krc_init(&_bootstrap_slot.rc);
	krc_get(&_bootstrap_slot.rc);     // for obj
	krc_get(&_bootstrap_slot.rc);     // to keep around
	krc_get(&_bootstrap_object.refs); // for slot
	krc_get(&_bootstrap_object.refs); // for vmap
	krc_get(&_bootstrap_object.refs); // to keep around
	_bootstrap_object.kslot = &_bootstrap_slot;
	_bootstrap_slot.obj = &_bootstrap_object;
	_bootstrap_object.flags = OF_KERNEL | OF_ALLOC;
	list_init(&_bootstrap_slot.spaces);

	arch_vm_map_object(NULL, &_bootstrap_vmap, _bootstrap_object.kslot);
	arch_object_map_slot(
	  NULL, &_bootstrap_object, &_bootstrap_slot, OBJSPACE_READ | OBJSPACE_WRITE);
	for(unsigned int idx = 0; idx < OBJ_MAXSIZE / mm_page_size(0); idx++) {
		arch_object_premap_page(&_bootstrap_object, idx, 0);
	}
	rb_insert(&slot_root, &_bootstrap_slot, struct slot, node, __slot_compar);
	skip_bootstrap_slot = oslot;
}

void slots_init(void)
{
	_max_slot = (2ul << arch_processor_physical_width()) / OBJ_MAXSIZE;
	/* skip both the bootstrap-alloc and the bootstrap (0) slot. */
	for(size_t i = 1; i < _max_slot; i++) {
		if(i == skip_bootstrap_slot)
			continue;
		struct slot *s = slabcache_alloc(&_sc_slot);
		s->num = i;
		s->next = slot_stack;
		list_init(&s->spaces);
		slot_stack = s;
		rb_insert(&slot_root, s, struct slot, node, __slot_compar);
	}
	printk(
	  "[slot] allocated %ld slots (%ld KB)\n", _max_slot, (_max_slot * sizeof(struct slot)) / 1024);
}

struct slot *slot_alloc(void)
{
	spinlock_acquire_save(&slot_lock);
	struct slot *s = slot_stack;
	if(!s) {
		panic("out of slots");
	}
	slot_stack = s->next;
	s->next = NULL;
	krc_init(&s->rc);
	spinlock_release_restore(&slot_lock);
	// printk("slot alloc %ld\n", s->num);
	return s;
}

struct slot *slot_lookup(size_t n)
{
	struct rbnode *node = rb_search(&slot_root, n, struct slot, node, __slot_compar_key);
	if(!node)
		return NULL;
	struct slot *s = rb_entry(node, struct slot, node);
	spinlock_acquire_save(&slot_lock);
	s = krc_get_unless_zero(&s->rc) ? s : NULL;
	spinlock_release_restore(&slot_lock);
	return s;
}

void slot_release(struct slot *s)
{
	if(krc_put_locked(&s->rc, &slot_lock)) {
		// printk("  FULL REL\n");
		s->next = slot_stack;
		slot_stack = s;
		spinlock_release_restore(&slot_lock);
	}
}

void object_space_map_slot(struct object_space *space, struct slot *slot, uint64_t flags)
{
	assert(slot->lock.data);
	if(!space)
		space =
		  current_thread && current_thread->active_sc ? &current_thread->active_sc->space : NULL;
	if(space) {
		struct slot_entry *se = slabcache_alloc(&_sc_slot_entry);
#if CONFIG_DEBUG
		for(struct list *e = list_iter_start(&slot->spaces); e != list_iter_end(&slot->spaces);
		    e = list_iter_next(e)) {
			struct slot_entry *xse = list_entry(e, struct slot_entry, entry);
			if(xse->space == space)
				panic("tried to remap a slot to a space it is already mapped in");
		}
#endif
		//	spinlock_acquire_save(&slot->lock);
		se->space = space;
		krc_get(&space->refs);
		list_insert(&slot->spaces, &se->entry);
	} else {
		//	spinlock_acquire_save(&slot->lock);
	}
	arch_object_map_slot(space, slot->obj, slot, flags);
	//	spinlock_release_restore(&slot->lock);
}

void object_space_release_slot(struct slot *slot)
{
	/* to call this, we'll first have no object referencing this slot. This means it'll never be
	 * mapped into an object_space while we're releasing it. */
	// printk("RELEASE %ld\n", slot->num);
	struct list *e, *n;
	spinlock_acquire_save(&slot->lock);
	for(e = list_iter_start(&slot->spaces); e != list_iter_end(&slot->spaces); e = n) {
		struct slot_entry *se = list_entry(e, struct slot_entry, entry);
		//	printk("  X: %ld: %p %p\n", slot->num, se, se->space);
		arch_object_unmap_slot(se->space, slot);
		n = list_iter_next(e);
		list_remove(e);
		/* TODO: put object space */
		slabcache_free(&_sc_slot_entry, se);
	}
	arch_object_unmap_slot(NULL, slot);
	spinlock_release_restore(&slot->lock);
}

void object_space_init(struct object_space *space)
{
	arch_object_space_init(space);
	arch_object_map_slot(
	  space, &_bootstrap_object, &_bootstrap_slot, OBJSPACE_READ | OBJSPACE_WRITE);
}

void object_space_destroy(struct object_space *space)
{
	arch_object_space_destroy(space);
}
