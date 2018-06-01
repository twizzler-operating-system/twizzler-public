#include <processor.h>

void arch_processor_enumerate()
{
	/* this is handled by initializers in madt.c */
}

static _Alignas(16) struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr;

void arch_processor_reset(void)
{
	idt_ptr.limit = 0;
	asm volatile("lidt (%0)"::"r"(&idt_ptr) : "memory");
	asm volatile("int $3");
}

