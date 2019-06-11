#pragma once
#define TWZ_GATE_SIZE 16
#define __TWZ_GATE(fn, g)                                                                          \
	__asm__(".section .gates, \"ax\", @progbits\n"                                                 \
	        ".global __twz_gate_" #fn "\n"                                                         \
	        ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
	        ".org " #g "*16, 0x90\n"                                                               \
	        "__twz_gate_" #fn ": call " #fn "\n"                                                   \
	        ".balign 16, 0x90\n"                                                                   \
	        ".previous");
#define TWZ_GATE(fn, g) __TWZ_GATE(fn, g)
#define TWZ_GATE_OFFSET (OBJ_NULLPAGE_SIZE + 0x200)

#include <twz/obj.h>
/* TODO: obj */
#define TWZ_GATE_CALL(obj, g) twz_ptr_local((void *)(g * TWZ_GATE_SIZE + TWZ_GATE_OFFSET))
