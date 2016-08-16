#include <interrupt.h>
#include <syscall.h>
#include <memory.h>
#include <debug.h>
void debug_puts(char *s);

static void (*sbi_set_timecmp)(unsigned long val) = (void *)(-1888);

static void _riscv_timer_handler(struct interrupt_frame *frame)
{
	uint64_t a;
	asm volatile("csrr %0, stime" : "=r"(a));
	sbi_set_timecmp(a + 0x10000);
}

static struct interrupt_handler timer_handler = {
	.fn = _riscv_timer_handler,
};

__initializer static void _init_timer(void)
{
	interrupt_register_handler(1, &timer_handler);
	uint64_t a;
	asm volatile("csrr %0, stime" : "=r"(a));
	sbi_set_timecmp(a + 0x1000);
}

void riscv_interrupt_entry(struct interrupt_frame *frame, uint64_t magic)
{
	uint64_t sc = frame->scause;
	if(sc & (1ull << 63)) {
		kernel_interrupt_entry(frame);
		asm volatile ( /* TODO: ack */
				"li t0, 1 << 1;"
				"csrw sip, x0;"
				::: "t0");
	} else {
		switch(sc & 15) {
			case 8:
				kernel_syscall_entry(frame);
				break;
			case 1: case 5: case 7:
				kernel_fault_entry(frame);
				break;
			case 3:
				kernel_debug_entry(frame);
				break;
			default:
				panic("unhandled exception");
				/* kernel_exception_entry(frame); */
		}
	}
	kernel_interrupt_postack(frame);
}

