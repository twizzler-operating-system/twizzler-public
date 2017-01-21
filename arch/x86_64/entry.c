#include <processor.h>

struct __attribute__((packed)) arch_exception_frame
{
	uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi;
	uint64_t int_no, err_code;
	uint64_t rip, cs, rflags, userrsp, ss;
};

struct __attribute__((packed)) x86_64_syscall_frame {
	uint64_t r10, r9, r8, rdx, rsi, rdi, rax, r11, rcx, rsp, cs;
};

void x86_64_exception_entry(struct arch_exception_frame *frame)
{
	/*if(frame->int_no < 32) {
	} else {
		//interrupt_entry(frame->int_no, frame->cs == 0x8 ? INTERRUPT_INKERNEL : 0);
	}
	*/
	printk("Interrupt! %d %lx\n", frame->int_no, frame->rip);
	x86_64_signal_eoi();
	for(;;);
	//if(frame->cs != 0x8) {
	//}
}

void x86_64_syscall_entry(struct x86_64_syscall_frame *frame)
{
}

