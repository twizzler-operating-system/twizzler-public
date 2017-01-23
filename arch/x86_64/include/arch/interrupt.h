#pragma once

#define MAX_INTERRUPT_VECTORS 256

static inline bool arch_interrupt_set(bool on)
{
	long old;
	asm volatile ("pushfq; pop %0;" : "=r"(old) :: "memory"); /* need full barrier for sync */
	if(on)
		asm volatile ("mfence; sti");
	else
		asm volatile ("cli; mfence");
	return !!(old & (1 << 9));
}
