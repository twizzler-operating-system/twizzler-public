#pragma once

#include <twz/_obj.h>

struct object {
	void *base;
};

#define twz_slot_to_base(s) ({ (void *)((s)*OBJ_MAXSIZE + OBJ_NULLPAGE_SIZE); })
#define twz_obj_base(s) ({ (void *)((uintptr_t)((s)->base) + OBJ_NULLPAGE_SIZE); })

#define TWZ_OC_HASHDATA 1
#define TWZ_OC_DFL_READ 2
#define TWZ_OC_DFL_WRITE 4
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);
