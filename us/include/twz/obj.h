#pragma once

#include <twz/_obj.h>
#include <twz/_slots.h>

struct object {
	void *base;
};

#define twz_slot_to_base(s) ({ (void *)((s)*OBJ_MAXSIZE + OBJ_NULLPAGE_SIZE); })
#define twz_obj_base(s) ({ (void *)((uintptr_t)((s)->base) + OBJ_NULLPAGE_SIZE); })

#define TWZ_OC_HASHDATA 1
#define TWZ_OC_DFL_READ 2
#define TWZ_OC_DFL_WRITE 4
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);

static inline void *__twz_ptr_lea(const struct object *o, const void *p)
{
	return (void *)((uintptr_t)o->base + (uintptr_t)p);
}

#define twz_ptr_lea(o, p) ({ (typeof(p)) __twz_ptr_lea((o), (p)); })

#define twz_ptr_local(p) ({ (typeof(p))((uintptr_t)p & (OBJ_MAXSIZE - 1)); })

#define twz_object_meta(o)                                                                         \
	({ (struct metainfo *)(((char *)o->base + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE)); })

int twz_object_open(struct object *obj, objid_t id, int flags);

#define SLOT_TO_VADDR(s) ({ (void *)((s)*OBJ_MAXSIZE); })

#define TWZ_OBJECT_INIT(s)                                                                         \
	{                                                                                              \
		.base = SLOT_TO_VADDR(s)                                                                   \
	}

void *twz_object_getext(struct object *obj, uint64_t tag);
