#pragma once

#include <arch/interrupt.h>
#include <lib/linkedlist.h>

struct interrupt_handler {
	void (*fn)(struct interrupt_frame *);
	struct linkedentry entry;
};

void kernel_interrupt_entry(void);
void interrupt_unregister_handler(int vector, struct interrupt_handler *handler);
void interrupt_register_handler(int vector, struct interrupt_handler *handler);
static inline void __interrupt_scoped_destruct(bool *set)
{
	arch_interrupt_set(*set);
}

#define interrupt_set_scope(x) \
	__cleanup(__interrupt_scoped_destruct) bool __concat(_int_status_, __COUNTER__) = arch_interrupt_set(x)

