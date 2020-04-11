#pragma once

#define twz_slot_to_base(s) ({ (void *)(SLOT_TO_VADDR(s) + OBJ_NULLPAGE_SIZE); })

static inline struct fotentry *_twz_object_get_fote(twzobj *obj, size_t e)
{
	struct metainfo *mi = twz_object_meta(obj);
	return (struct fotentry *)((char *)mi - sizeof(struct fotentry) * e);
}

#define TWZ_OBJ_VALID 1
#define TWZ_OBJ_NORELEASE 2
#define TWZ_OBJ_CACHE 64
