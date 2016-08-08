#include <interrupt.h>
void debug_puts(char *s);
void arch_interrupt_entry(struct interrupt_frame *frame, uint64_t magic)
{
	kernel_interrupt_entry(frame);
	//printk("Int %lx %lx %lx %lx %lx\n", frame->sstatus, frame->scause, frame->sepc, frame->sip, frame->sbadaddr);
	asm volatile (
			"li t0, 1 << 1;"
			"csrc sip, t0;"
			::: "t0");
	kernel_interrupt_postack(frame);
}

