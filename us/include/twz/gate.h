#pragma once
#define TWZ_GATE_SIZE 16
#define __TWZ_GATE(fn, g)                                                                          \
	__asm__(".section .gates, \"ax\", @progbits\n"                                                 \
	        ".global __twz_gate_" #fn "\n"                                                         \
	        ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
	        ".org " #g "*16, 0x90\n"                                                               \
	        "__twz_gate_" #fn ": call " #fn "\n"                                                   \
	        "ret\n"                                                                                \
	        ".balign 16, 0x90\n"                                                                   \
	        ".previous");
#define TWZ_GATE(fn, g) __TWZ_GATE(fn, g)
#define TWZ_GATE_OFFSET (OBJ_NULLPAGE_SIZE + 0x200)

#include <twz/obj.h>
#define TWZ_GATE_CALL(_obj, g)                                                                     \
	({                                                                                             \
		struct object *obj = _obj;                                                                 \
		(void *)((obj ? (uintptr_t)twz_obj_base(obj) : 0ull) + g * TWZ_GATE_SIZE                   \
		         + TWZ_GATE_OFFSET);                                                               \
	})
