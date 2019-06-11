#pragma once
#define TWZ_GATE(fn, g)                                                                            \
	__asm__(".section .gates, \"ax\", @progbits\n"                                                 \
	        ".global __twz_gate_" #fn "\n"                                                         \
	        ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
	        ".org " #g "*16, 0x90\n"                                                               \
	        "__twz_gate_" #fn ": call " #fn "\n"                                                   \
	        ".balign 16, 0x90\n"                                                                   \
	        ".previous");

#define TWZ_GATE_OFFSET (OBJ_NULLPAGE_SIZE + 0x200)
