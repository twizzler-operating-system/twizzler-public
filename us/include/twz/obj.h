#pragma once

#include <twz/_obj.h>

struct object {
	void *base;
};

#define twz_slot_to_base(s) ({ (void *)((s)*OBJ_MAXSIZE + OBJ_NULLPAGE_SIZE); })
#define twz_obj_base(s) ({ (void *)((uintptr_t)((s)->base) + OBJ_NULLPAGE_SIZE); })
