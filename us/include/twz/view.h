#pragma once
#include <stdint.h>
#include <twz/_objid.h>
#include <twz/_slots.h>
#include <twz/_types.h>
#include <twz/_view.h>
#include <twz/mutex.h>

struct __viewrepr_bucket {
	objid_t id;
	uint64_t slot;
	uint32_t flags;
	int32_t chain;
	uint64_t info;
	uint64_t refs;
};

struct twzview_repr {
	struct kso_hdr hdr;
	struct viewentry ves[TWZSLOT_MAX_SLOT + 1];
	struct mutex lock;
	uint8_t bitmap[(TWZSLOT_MAX_SLOT + 1) / 8];
	struct __viewrepr_bucket buckets[TWZSLOT_MAX_SLOT + 1];
	struct __viewrepr_bucket chain[TWZSLOT_MAX_SLOT + 1];
};

_Static_assert(offsetof(struct twzview_repr, ves) == __VE_OFFSET,
  "Offset of ves must be equal to __VE_OFFSET");

twzobj;
void twz_view_get(twzobj *obj, size_t slot, objid_t *target, uint32_t *flags);
void twz_view_set(twzobj *obj, size_t slot, objid_t target, uint32_t flags);
void twz_view_fixedset(twzobj *obj, size_t slot, objid_t target, uint32_t flags);
void twz_view_object_init(twzobj *obj);

__must_check int twz_vaddr_to_obj(const void *v, objid_t *id, uint32_t *fl);
__must_check ssize_t twz_view_allocate_slot(twzobj *obj, objid_t id, uint32_t flags);
void twz_view_release_slot(twzobj *obj, objid_t id, uint32_t flags, size_t slot);

#define VIEW_CLONE_ENTRIES 1
#define VIEW_CLONE_BITMAP 2

__must_check int twz_view_clone(twzobj *old,
  twzobj *nobj,
  int flags,
  bool (*fn)(twzobj *, size_t, objid_t, uint32_t, objid_t *, uint32_t *));
