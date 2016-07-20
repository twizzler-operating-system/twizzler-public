#pragma once

static inline bool arch_interrupt_set(bool on)
{
	uint64_t old;
	if(on) {
		asm volatile(
				"li t0, 1;"
				"csrrs %0, sstatus, t0;"
				: "=r"(old) :: "t1");
	} else {
		asm volatile(
				"li t0, 1;"
				"csrrc %0, sstatus, t0;"
				: "=r"(old) :: "t1");
	}
	return !!(old & 1);
}

