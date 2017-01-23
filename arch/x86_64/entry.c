#include <processor.h>
#include <thread.h>

void x86_64_exception_entry(struct x86_64_exception_frame *frame, bool was_userspace)
{
	if(was_userspace) {
		current_thread->arch.was_syscall = false;
	}

	printk("Interrupt! %d %lx (%p)\n", frame->int_no, frame->rip, &current_thread->processor->arch.curr);
	x86_64_signal_eoi();
	if(was_userspace) {
		thread_schedule_resume();
	}
}

void x86_64_syscall_entry(struct x86_64_syscall_frame *frame)
{
	current_thread->arch.was_syscall = true;
	printk("syscall! %lx\n", frame->rax);
	thread_schedule_resume();
}

extern void x86_64_resume_userspace(void *);
extern void x86_64_resume_userspace_interrupt(void *);
void arch_thread_resume(struct thread *thread)
{
	thread->processor->arch.curr = thread;
	thread->processor->arch.tcb = (void *)((uint64_t)&thread->arch.syscall + sizeof(struct x86_64_syscall_frame));
	thread->processor->arch.tss.esp0 = (void *)((uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame));

	if(thread->arch.was_syscall)
		x86_64_resume_userspace(&thread->arch.syscall);
	else {
		x86_64_resume_userspace_interrupt(&thread->arch.exception);
	}
}

