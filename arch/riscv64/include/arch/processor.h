#pragma once

static inline void arch_processor_relax(void)
{
	asm volatile("nop");
}

