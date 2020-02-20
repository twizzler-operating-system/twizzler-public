#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <twz/_objid.h>
#include <twz/_slots.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/thread.h>
#include <twz/view.h>

#include <twz/_err.h>
#include <twz/_types.h>

#include <twz/debug.h>

void twz_view_object_init(twzobj *obj)
{
	*obj = TWZ_OBJECT_INIT(TWZSLOT_CVIEW);
}

int twz_view_set(twzobj *obj, size_t slot, objid_t target, uint32_t flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -EINVAL;
	}
	struct viewentry *ves = obj ? ((struct twzview_repr *)twz_object_base(obj))->ves
	                            : ((struct twzview_repr *)twz_slot_to_base(TWZSLOT_CVIEW))->ves;
	uint32_t old = atomic_fetch_and(&ves[slot].flags, ~VE_VALID);
	ves[slot].id = target;
	ves[slot].res0 = 0;
	ves[slot].res1 = 0;
	atomic_store(&ves[slot].flags, flags | VE_VALID);

#warning "TODO: seems like this happens too much?"
	if((old & VE_VALID)) {
		struct sys_invalidate_op op = {
			.offset = slot * OBJ_MAXSIZE,
			.length = 1,
			.flags = KSOI_VALID | KSOI_CURRENT,
			.id = KSO_CURRENT_VIEW,
		};
		return sys_invalidate(&op, 1);
	}

	return 0;
}

int twz_view_fixedset(twzobj *obj, size_t slot, objid_t target, uint32_t flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -EINVAL;
	}
	flags &= ~VE_FIXED;
	struct viewentry *ves = obj ? ((struct twzthread_repr *)twz_object_base(obj))->fixed_points
	                            : (struct viewentry *)twz_thread_repr_base()->fixed_points;
	uint32_t old = atomic_fetch_and(&ves[slot].flags, ~VE_VALID);
	ves[slot].id = target;
	ves[slot].res0 = 0;
	ves[slot].res1 = 0;
	atomic_store(&ves[slot].flags, flags | VE_VALID);

	if(old & VE_VALID) {
		struct sys_invalidate_op op = {
			.offset = slot * OBJ_MAXSIZE,
			.length = 1,
			.flags = KSOI_VALID | KSOI_CURRENT,
			.id = KSO_CURRENT_VIEW,
		};
		return sys_invalidate(&op, 1);
	}

	return 0;
}

int twz_view_get(twzobj *obj, size_t slot, objid_t *target, uint32_t *flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -EINVAL;
	}
	struct viewentry *ves;
	uint32_t extra_flags = 0;
	if(obj) {
		ves = &(((struct twzview_repr *)twz_object_base(obj))->ves[slot]);
	} else {
		struct twzthread_repr *tr = twz_thread_repr_base();
		ves = &tr->fixed_points[slot];
		extra_flags |= VE_FIXED;
		if(!(ves->flags & VE_VALID)) {
			ves = &(((struct twzview_repr *)twz_slot_to_base(TWZSLOT_CVIEW))->ves[slot]);
			extra_flags = 0;
		}
	}
	if(flags)
		*flags = atomic_load(&ves->flags) | extra_flags;
	if(target)
		*target = ves->id;
	return 0;
}

int twz_vaddr_to_obj(const void *v, objid_t *id, uint32_t *fl)
{
	uint32_t tf;
	int r = twz_view_get(NULL, VADDR_TO_SLOT(v), id, &tf);
	if(!r && !(tf & VE_VALID))
		return ENOENT;
	if(fl)
		*fl = tf;
	return r;
}

static struct __viewrepr_bucket *__lookup_bucket(struct twzview_repr *v, objid_t id, uint32_t flags)
{
	int32_t idx = id % (TWZSLOT_MAX_SLOT + 1);
	struct __viewrepr_bucket *b = v->buckets;
	do {
		if(b[idx].id == 0)
			return NULL;
		if(b[idx].id == id && b[idx].flags == flags) {
			return &b[idx];
		}
		idx = b[idx].chain - 1;
		b = v->chain;
	} while(idx != -1);
	return NULL;
}

static struct __viewrepr_bucket *__insert_obj(struct twzview_repr *v,
  objid_t id,
  uint32_t flags,
  size_t slot)
{
	int32_t idx = id % (TWZSLOT_MAX_SLOT + 1);
	struct __viewrepr_bucket *b = v->buckets;
	struct __viewrepr_bucket *pb = NULL;
	do {
		if(b[idx].id == 0) {
			b[idx].id = id;
			b[idx].slot = slot;
			b[idx].flags = flags;
			b[idx].refs = 1;
			return &b[idx];
		}
		pb = &b[idx];
		idx = b[idx].chain - 1;
		b = v->chain;
	} while(idx != -1);
	for(size_t i = 0; i <= TWZSLOT_MAX_SLOT; i++) {
		if(b[i].id == 0 && b[i].chain == 0) {
			b[i].id = id;
			b[i].flags = flags;
			b[i].slot = slot;
			b[i].refs = 1;
			b[i].chain = 0;
			pb->chain = i + 1;
			return &b[i];
		}
	}
	return NULL;
}

ssize_t __alloc_slot(struct twzview_repr *v)
{
	for(size_t i = TWZSLOT_ALLOC_START / 8; i <= TWZSLOT_ALLOC_MAX / 8; i++) {
		size_t idx = i / 8;
		if(v->bitmap[idx] != 0xff) {
			for(int bit = 0; bit < 8; bit++) {
				if(!(v->bitmap[idx] & (1 << bit))) {
					v->bitmap[idx] |= (1 << bit);
					return idx * 8 + bit;
				}
			}
		}
	}
	return -ENOSPC;
}

#include <assert.h>
void twz_view_release_slot(twzobj *obj, objid_t id, uint32_t flags, size_t slot)
{
	struct twzview_repr *v = obj ? (struct twzview_repr *)twz_object_base(obj)
	                             : (struct twzview_repr *)twz_slot_to_base(TWZSLOT_CVIEW);

	mutex_acquire(&v->lock);
	struct __viewrepr_bucket *b = __lookup_bucket(v, id, flags);
	assert(b);
	assert(b->slot == slot);

	if(b->refs-- == 0) {
		assert(0);
	}

	mutex_release(&v->lock);
}

ssize_t twz_view_allocate_slot(twzobj *obj, objid_t id, uint32_t flags)
{
	struct twzview_repr *v = obj ? (struct twzview_repr *)twz_object_base(obj)
	                             : (struct twzview_repr *)twz_slot_to_base(TWZSLOT_CVIEW);

	mutex_acquire(&v->lock);

	struct __viewrepr_bucket *b = __lookup_bucket(v, id, flags);
	ssize_t slot;
	if(b) {
		slot = b->slot;
		assert(b->refs > 0);
		b->refs++;
	} else {
		slot = __alloc_slot(v);
		if(slot < 0) {
			mutex_release(&v->lock);
			return slot;
		}
		b = __insert_obj(v, id, flags, slot);
		int r;
		if((r = twz_view_set(obj, slot, id, flags)) < 0)
			return r;
	}

	mutex_release(&v->lock);
	return slot;
}
