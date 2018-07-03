#pragma once
#include <stdatomic.h>
#include <stddef.h>
#include <twz.h>
#include <twzslots.h>
#include <twzobj.h>
#include <twzsys.h>

#define VE_READ 1
#define VE_WRITE 2
#define VE_EXEC 4
#define VE_VALID 0x80000000

#define MAX_SLOT 0x1fff0
#define OBJ_SLOTSIZE (1024ul * 1024 * 1024)

struct virtentry {
	objid_t id;
	uint64_t res0;
	_Atomic uint32_t flags;
	uint32_t res1;
}__attribute__((packed));

#define twz_current_viewid() twz_view_virt_to_objid(NULL, twz_slot_to_base(TWZSLOT_CVIEW))
objid_t twz_view_virt_to_objid(struct object *obj, void *p);

static inline int twz_view_invl(objid_t id, size_t start, size_t end, int flags)
{
	return fbsd_twistie_invlview(ID_LO(id), ID_HI(id), start, end, flags);
}

/* TODO: invalidate */
static inline int twz_view_set(struct object *obj, size_t slot, objid_t target, uint32_t flags)
{
	struct virtentry *ves = obj
		? (struct virtentry *)twz_ptr_base(obj)
		: (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	uint32_t old = atomic_fetch_and(&ves[slot].flags, ~VE_VALID);
	ves[slot].id = target;
	ves[slot].res0 = 0;
	ves[slot].res1 = 0;
	atomic_store(&ves[slot].flags, flags | VE_VALID);

	if(old & VE_VALID) {
		twz_view_invl(twz_view_virt_to_objid(obj, twz_slot_to_base(TWZSLOT_CVIEW)),
				slot, slot, 0);
	}

	return 0;
}

static inline int twz_view_tryset(struct object *obj, size_t slot, objid_t target, uint32_t flags)
{
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
	struct virtentry *ves = obj
		? (struct virtentry *)twz_ptr_base(obj)
		: (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	*flags = atomic_load(&ves[slot].flags);
	*target = ves[slot].id;
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
	return fbsd_twistie_setview(ID_LO(v), ID_HI(v), flags);
}

int twz_view_blank(struct object *obj);

