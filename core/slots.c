#include <lib/rb.h>
#include <processor.h>
#include <slab.h>
#include <slots.h>
#include <spinlock.h>

#include <twz/_obj.h>

static DECLARE_SLABCACHE(_sc_slot, sizeof(struct slot), NULL, NULL, NULL);

static struct rbroot slot_root = RBINIT;

static struct slot *slot_stack = NULL;
static struct spinlock slot_lock = SPINLOCK_INIT;

static size_t _max_slot = 0;

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

__initializer static void _slots_init(void)
{
	_max_slot = (2ul << arch_processor_physical_width()) / OBJ_MAXSIZE;
	for(size_t i = 0; i < _max_slot; i++) {
		struct slot *s = slabcache_alloc(&_sc_slot);
		s->num = i;
		s->next = slot_stack;
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
	slot_stack = s->next;
	s->next = NULL;
	krc_init(&s->rc);
	spinlock_release_restore(&slot_lock);
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
		s->next = slot_stack;
		slot_stack = s;
		spinlock_release_restore(&slot_lock);
	}
}
