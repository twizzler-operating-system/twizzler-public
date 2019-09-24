#pragma once

#include <arch/interrupt.h>
#include <lib/list.h>

struct interrupt_handler {
	void (*fn)(int, struct interrupt_handler *);
	struct list entry;
};

enum iv_priority {
	IVP_NORMAL,
	IVP_UNIQUE,
};

#define INTERRUPT_ALLOC_REQ_VALID 1
#define INTERRUPT_ALLOC_REQ_ENABLED 2

struct interrupt_alloc_req {
	struct interrupt_handler handler;
	uint8_t flags;
	enum iv_priority pri;
	int vec;
};

int interrupt_allocate_vectors(size_t count, struct interrupt_alloc_req *req);
void kernel_interrupt_entry(int);
void interrupt_unregister_handler(int vector, struct interrupt_handler *handler);
void interrupt_register_handler(int vector, struct interrupt_handler *handler);
void arch_interrupt_unmask(int v);
void arch_interrupt_mask(int v);
static inline void __interrupt_scoped_destruct(bool *set)
{
	arch_interrupt_set(*set);
}

#define interrupt_set_scope(x)                                                                     \
	__cleanup(__interrupt_scoped_destruct) bool __concat(_int_status_, __COUNTER__) =              \
	  arch_interrupt_set(x)
