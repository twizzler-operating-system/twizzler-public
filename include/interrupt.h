#pragma once

#include <arch/interrupt.h>

static inline void __interrupt_scoped_destruct(bool *set)
{
	arch_interrupt_set(*set);
}

#define interrupt_set_scope(x) \
	__cleanup(__interrupt_scoped_destruct) bool __concat(_int_status_, __COUNTER__) = arch_interrupt_set(x)
