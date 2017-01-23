#include <processor.h>
#include <thread.h>

void x86_64_exception_entry(struct x86_64_exception_frame *frame, bool was_userspace)
{
	current_thread->arch.was_syscall = false;

	uint64_t t;
	asm("movq %%rsp, %0" :"=r"(t));
	printk("Interrupt! %d %lx (%p) :: %lx\n", frame->int_no, frame->rip, &current_thread->processor->arch.curr, t);
	asm volatile("xchg %%bx, %%bx" ::: "bx");
	if(was_userspace) {
		x86_64_resume(current_thread);
	}
	return;
	/*if(frame->int_no < 32) {
	} else {
		//interrupt_entry(frame->int_no, frame->cs == 0x8 ? INTERRUPT_INKERNEL : 0);
	}
	*/
	printk("current_thread = %p\n", current_thread);
	for(;;);
	//x86_64_signal_eoi();
	//for(;;);
	//if(frame->cs != 0x8) {
	//}
}

void x86_64_syscall_entry(struct x86_64_syscall_frame *frame)
{
	current_thread->arch.was_syscall = true;
	x86_64_resume(current_thread);
}

extern void x86_64_resume_userspace(void *);
extern void x86_64_resume_userspace_interrupt(void *);
void x86_64_resume(struct thread *thread)
{
	printk("::- %p %p %p (%d)\n", thread->processor->arch.curr, thread, current_thread, thread->arch.was_syscall);
	thread->processor->arch.curr = thread;
	thread->processor->arch.tcb = (void *)((uint64_t)&thread->arch.syscall + sizeof(struct x86_64_syscall_frame));
	thread->processor->arch.tss.esp0 = (void *)((uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame));
	if(thread->arch.was_syscall)
		x86_64_resume_userspace(&thread->arch.syscall);
	else {
		printk(":: %lx\n", thread->arch.exception.cs);
		x86_64_resume_userspace_interrupt(&thread->arch.exception);
	}
}

