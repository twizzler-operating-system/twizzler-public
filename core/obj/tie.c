#include <object.h>
#include <slab.h>
static DECLARE_SLABCACHE(sc_objtie, sizeof(struct object_tie), NULL, NULL, NULL);

static int __objtie_compar_key(struct object_tie *a, objid_t n)
{
	if(a->child->id > n)
		return 1;
	else if(a->child->id < n)
		return -1;
	return 0;
}

static int __objtie_compar(struct object_tie *a, struct object_tie *b)
{
	return __objtie_compar_key(a, b->child->id);
}

int obj_untie(struct object *parent, struct object *child)
{
	// printk("untying " IDFMT " -> " IDFMT "\n", IDPR(child->id), IDPR(parent->id));
	struct object *rel = NULL;
	spinlock_acquire_save(&parent->lock);

	struct rbnode *node =
	  rb_search(&parent->ties_root, child->id, struct object_tie, node, __objtie_compar_key);
	struct object_tie *tie;
	if(!node) {
		spinlock_release_restore(&parent->lock);
		return -ENOENT;
	}
	tie = rb_entry(node, struct object_tie, node);
	if(--tie->count == 0) {
		rb_delete(&tie->node, &parent->ties_root);
		rel = tie->child;
		slabcache_free(&sc_objtie, tie);
	}

	spinlock_release_restore(&parent->lock);
	obj_put(rel);
	return 0;
}

void obj_tie(struct object *parent, struct object *child)
{
	// printk("tying " IDFMT " -> " IDFMT "\n", IDPR(child->id), IDPR(parent->id));
	spinlock_acquire_save(&parent->lock);

	struct rbnode *node =
	  rb_search(&parent->ties_root, child->id, struct object_tie, node, __objtie_compar_key);
	struct object_tie *tie;
	if(!node) {
		tie = slabcache_alloc(&sc_objtie);
		krc_get(&child->refs);
		tie->child = child;
		tie->count = 1;
		rb_insert(&parent->ties_root, tie, struct object_tie, node, __objtie_compar);
	} else {
		tie = rb_entry(node, struct object_tie, node);
		tie->count++;
	}

	spinlock_release_restore(&parent->lock);
}
