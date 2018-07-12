#pragma once
#include <stdatomic.h>
#include <stddef.h>
#include <twz.h>
#include <twzslots.h>
#include <twzobj.h>
#include <twzsys.h>
#include <mutex.h>

#define VE_READ 1
#define VE_WRITE 2
#define VE_EXEC 4
#define VE_VALID 0x80000000

#define OBJ_SLOTSIZE (1024ul * 1024 * 1024)

#define NUM_SLOTS_PER_OBJECT 4

struct virtentry {
	objid_t id;
	uint64_t res0;
	_Atomic uint32_t flags;
	uint32_t res1;
}__attribute__((packed));

struct __view_repr_tblent {
	objid_t id;
	ssize_t slot;
};

struct __view_repr {
	struct virtentry ves[TWZSLOT_MAX_SLOT+1];
	struct mutex lock;
	uint8_t obj_bitmap[TWZSLOT_MAX_SLOT / (8 * NUM_SLOTS_PER_OBJECT) + 1];
	size_t tblsz;
	struct __view_repr_tblent table[];
};

#define twz_current_viewid() twz_view_virt_to_objid(NULL, twz_slot_to_base(TWZSLOT_CVIEW))
objid_t twz_view_virt_to_objid(struct object *obj, void *p);

static inline int twz_view_set(struct object *obj, size_t slot, objid_t target, uint32_t flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -TE_INVALID;
	}
	struct virtentry *ves = obj
		? (struct virtentry *)twz_ptr_base(obj)
		: (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	uint32_t old = atomic_fetch_and(&ves[slot].flags, ~VE_VALID);
	ves[slot].id = target;
	ves[slot].res0 = 0;
	ves[slot].res1 = 0;
	atomic_store(&ves[slot].flags, flags | VE_VALID);

	if(old & VE_VALID) {
		struct sys_invalidate_op op = {
			.id = twz_current_viewid(),
			.offset = slot * OBJ_SLOTSIZE,
			.length = 1,
			.flags = KSOI_VALID,
		};
		sys_invalidate(&op, 1);
	}

	return 0;
}

static inline int twz_view_tryset(struct object *obj, size_t slot, objid_t target, uint32_t flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -TE_INVALID;
	}
	struct virtentry *ves = obj
		? (struct virtentry *)twz_ptr_base(obj)
		: (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	if(atomic_fetch_or(&ves[slot].flags, VE_VALID) & VE_VALID) {
		return -1;
	}
	ves[slot].flags = VE_VALID;
	ves[slot].id = target;
	ves[slot].flags = flags | VE_VALID;
	return 0;
}

static inline int twz_view_get(struct object *obj, size_t slot, objid_t *target, uint32_t *flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -TE_INVALID;
	}
	struct virtentry *ves = obj
		? (struct virtentry *)twz_ptr_base(obj)
		: (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	if(flags)  *flags = atomic_load(&ves[slot].flags);
	if(target) *target = ves[slot].id;
	return 0;
}

static inline void twz_view_copy(struct object *dest, struct object *src, size_t slot)
{
	objid_t tmpid;
	uint32_t tmpfl;
	twz_view_get(src, slot, &tmpid, &tmpfl);
	twz_view_set(dest, slot, tmpid, tmpfl);
}

static inline int twz_view_switch(objid_t v, int flags)
{
	return sys_attach(0, v, flags);
}

int twz_view_blank(struct object *obj);

