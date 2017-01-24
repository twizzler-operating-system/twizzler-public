#include <processor.h>
#include <thread.h>
#include <arch/x86_64-msr.h>

static void x86_64_change_fpusse_allow(bool enable)
{
	uint64_t tmp;
	asm volatile("mov %%cr0, %0" : "=r"(tmp));
	tmp = enable ? (tmp & ~(1 << 2)) : (tmp | (1 << 2));
	asm volatile("mov %0, %%cr0" :: "r"(tmp));
	asm volatile("mov %%cr4, %0" : "=r"(tmp));
	tmp = enable ? (tmp | (1 << 9)) : (tmp & ~(1 << 9));
	asm volatile("mov %0, %%cr4" :: "r"(tmp));
}

void x86_64_exception_entry(struct x86_64_exception_frame *frame, bool was_userspace)
{
	if(was_userspace) {
		current_thread->arch.was_syscall = false;
		if((frame->int_no == 6 || frame->int_no == 7) && current_thread && !current_thread->arch.usedfpu) {
			/* we're emulating FPU instructions / disallowing SSE. Set a flag,
			 * and allow the thread to do its thing */
			current_thread->arch.usedfpu = true;
			x86_64_change_fpusse_allow(true);
			asm volatile ("finit"); /* also, we may need to clear the FPU state */
		}
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
	struct thread *old = current_thread;
	thread->processor->arch.curr = thread;
	thread->processor->arch.tcb = (void *)((uint64_t)&thread->arch.syscall + sizeof(struct x86_64_syscall_frame));
	thread->processor->arch.tss.esp0 = ((uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame));

	/* restore segment bases for new thread */
	x86_64_wrmsr(X86_MSR_FS_BASE, thread->arch.fs & 0xFFFFFFFF, (thread->arch.fs >> 32) & 0xFFFFFFFF);
	x86_64_wrmsr(X86_MSR_KERNEL_GS_BASE, thread->arch.gs & 0xFFFFFFFF, (thread->arch.gs >> 32) & 0xFFFFFFFF);

	if(old && old->arch.usedfpu) {
		/* store FPU sate */
		asm volatile ("fxsave (%0)" 
				:: "r" (old->arch.fpu_data) : "memory");
	}

	x86_64_change_fpusse_allow(thread->arch.usedfpu);
	if(thread->arch.usedfpu) {
		/* restore FPU state */
		asm volatile ("fxrstor (%0)" 
				:: "r" (thread->arch.fpu_data) : "memory");
	}

	if(thread->arch.was_syscall)
		x86_64_resume_userspace(&thread->arch.syscall);
	else {
		x86_64_resume_userspace_interrupt(&thread->arch.exception);
	}
}

