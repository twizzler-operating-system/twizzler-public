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
};

struct twzview_repr {
	struct viewentry ves[TWZSLOT_MAX_SLOT + 1];
	struct mutex lock;
	uint8_t bitmap[(TWZSLOT_MAX_SLOT + 1) / 8];
	struct __viewrepr_bucket buckets[TWZSLOT_MAX_SLOT + 1];
	struct __viewrepr_bucket chain[TWZSLOT_MAX_SLOT + 1];
};

struct object;
int twz_view_get(struct object *obj, size_t slot, objid_t *target, uint32_t *flags);
int twz_view_set(struct object *obj, size_t slot, objid_t target, uint32_t flags);
int twz_view_fixedset(struct object *obj, size_t slot, objid_t target, uint32_t flags);
int twz_vaddr_to_obj(const void *v, objid_t *id, uint32_t *fl);
ssize_t twz_view_allocate_slot(struct object *obj, objid_t id, uint32_t flags);
