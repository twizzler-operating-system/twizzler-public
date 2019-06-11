#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <twz/_objid.h>
#include <twz/_slots.h>
#include <twz/_view.h>
#include <twz/obj.h>
#include <twz/sys.h>

#include <twz/_err.h>

int twz_view_set(struct object *obj, size_t slot, objid_t target, uint32_t flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -EINVAL;
	}
	struct viewentry *ves = obj ? (struct viewentry *)twz_obj_base(obj)
	                            : (struct viewentry *)twz_slot_to_base(TWZSLOT_CVIEW);
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

int twz_view_get(struct object *obj, size_t slot, objid_t *target, uint32_t *flags)
{
	if(slot > TWZSLOT_MAX_SLOT) {
		return -EINVAL;
	}
	struct viewentry *ves = obj ? (struct viewentry *)twz_obj_base(obj)
	                            : (struct viewentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	if(flags)
		*flags = atomic_load(&ves[slot].flags);
	if(target)
		*target = ves[slot].id;
	return 0;
}

int twz_vaddr_to_obj(const void *v, objid_t *id, uint32_t *fl)
{
	return twz_view_get(NULL, VADDR_TO_SLOT(v), id, fl);
}
