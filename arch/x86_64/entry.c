#include <arch/x86_64-msr.h>
#include <arch/x86_64-vmx.h>
#include <clksrc.h>
#include <processor.h>
#include <secctx.h>
#include <syscall.h>
#include <thread.h>

void x86_64_signal_eoi(void);

void x86_64_ipi_tlb_shootdown(void)
{
	asm volatile("mov %%cr3, %%rax; mov %%rax, %%cr3; mfence;" ::: "memory", "rax");
	current_thread->processor->stats.shootdowns++;
	processor_ipi_finish();
}

void x86_64_ipi_resume(void)
{
	/* don't need to do anything, just handle the interrupt */
}

void x86_64_ipi_halt(void)
{
	processor_shutdown();
	asm volatile("cli; jmp .;" ::: "memory");
}

/*
static void x86_64_change_fpusse_allow(bool enable)
{
    register uint64_t tmp;
    asm volatile("mov %%cr0, %0" : "=r"(tmp));
    tmp = enable ? (tmp & ~(1 << 2)) : (tmp | (1 << 2));
    asm volatile("mov %0, %%cr0" ::"r"(tmp));
    asm volatile("mov %%cr4, %0" : "=r"(tmp));
    tmp = enable ? (tmp | (1 << 9)) : (tmp & ~(1 << 9));
    asm volatile("mov %0, %%cr4" ::"r"(tmp));
}
*/

__noinstrument void x86_64_exception_entry(struct x86_64_exception_frame *frame,
  bool was_userspace,
  bool ignored)
{
	if(!ignored) {
		if(was_userspace) {
			current_thread->arch.was_syscall = false;
		}

		if((frame->int_no == 6 || frame->int_no == 7)) {
			if(!was_userspace) {
				panic("floating-point operations used in kernel-space");
			}
			panic("NI - FP exception");
		} else if(frame->int_no == 14) {
			/* page fault */
			uint64_t cr2;
			asm volatile("mov %%cr2, %0" : "=r"(cr2)::"memory");
			int flags = 0;
			if(frame->err_code & 1) {
				flags |= FAULT_ERROR_PERM;
			} else {
				flags |= FAULT_ERROR_PRES;
			}
			if(frame->err_code & (1 << 1)) {
				flags |= FAULT_WRITE;
			}
			if(frame->err_code & (1 << 2)) {
				flags |= FAULT_USER;
			}
			if(frame->err_code & (1 << 4)) {
				flags |= FAULT_EXEC;
			}
			if(!was_userspace) {
				panic("kernel-mode page fault to address %lx\n"
				      "    from %lx: %s while in %s-mode: %s\n",
				  cr2,
				  frame->rip,
				  flags & FAULT_EXEC ? "ifetch" : (flags & FAULT_WRITE ? "write" : "read"),
				  flags & FAULT_USER ? "user" : "kernel",
				  flags & FAULT_ERROR_PERM ? "present" : "non-present");
			}
			kernel_fault_entry(frame->rip, cr2, flags);
		} else if(frame->int_no == 20) {
			/* #VE */
			x86_64_virtualization_fault(current_processor);
		} else if(frame->int_no < 32) {
			if(was_userspace) {
				struct fault_exception_info info = {
					.ip = frame->rip,
					.code = frame->int_no,
					.arg0 = frame->err_code,
				};
				thread_raise_fault(current_thread, FAULT_EXCEPTION, &info, sizeof(info));
			} else {
				if(frame->int_no == 3) {
					printk("[debug]: recv debug interrupt\n");
					kernel_debug_entry();
				}
				panic("kernel exception: %ld, from %lx\n", frame->int_no, frame->rip);
			}
		} else if(frame->int_no == PROCESSOR_IPI_HALT) {
			x86_64_ipi_halt();
		} else if(frame->int_no == PROCESSOR_IPI_SHOOTDOWN) {
			x86_64_ipi_tlb_shootdown();
		} else if(frame->int_no == PROCESSOR_IPI_RESUME) {
			x86_64_ipi_resume();
		} else {
#if 0
			printk("INTERRUPT : %ld (%d ; %d)\n",
			  frame->int_no,
			  was_userspace,
			  current_thread->arch.was_syscall);
#endif
			kernel_interrupt_entry(frame->int_no);
		}
	}

	if(frame->int_no >= 32) {
		current_thread->processor->stats.ext_intr++;
		x86_64_signal_eoi();
	} else {
		current_thread->processor->stats.int_intr++;
	}
	if(was_userspace) {
		thread_schedule_resume();
	}
	/* if we aren't in userspace, we just return and the kernel_exception handler will
	 * properly restore the frame and continue */
}

extern long (*syscall_table[])();
__noinstrument void x86_64_syscall_entry(struct x86_64_syscall_frame *frame)
{
	current_thread->arch.was_syscall = true;
	arch_interrupt_set(true);
	current_thread->processor->stats.syscalls++;
#if CONFIG_PRINT_SYSCALLS
	if(frame->rax != SYS_DEBUG_PRINT)
		printk("%ld: SYSCALL %ld (%lx)\n", current_thread->id, frame->rax, frame->rcx);
#endif
	if(frame->rax < NUM_SYSCALLS) {
		if(syscall_table[frame->rax]) {
			long r = syscall_prelude(frame->rax);
			if(!r) {
				frame->rax = syscall_table[frame->rax](
				  frame->rdi, frame->rsi, frame->rdx, frame->r8, frame->r9, frame->r10);
			} else {
				frame->rax = r;
			}
			r = syscall_epilogue(frame->rax);
			if(r) {
				panic("NI - non-zero return code from syscall epilogue");
			}
		}
		frame->rdx = 0;
	} else {
		frame->rax = -EINVAL;
		frame->rdx = 0;
	}

	arch_interrupt_set(false);
	thread_schedule_resume();
}

void secctx_switch(int i)
{
	current_thread->active_sc = current_thread->attached_scs[i];
	if(!current_thread->active_sc) {
		return;
	}
	x86_64_secctx_switch(current_thread->active_sc);
}

extern void x86_64_resume_userspace(void *);
extern void x86_64_resume_userspace_interrupt(void *);
__noinstrument void arch_thread_resume(struct thread *thread, uint64_t timeout)
{
	// printk("resume %ld\n", thread->id);
	struct thread *old = current_thread;
	thread->processor->arch.curr = thread;
	thread->processor->arch.tcb =
	  (void *)((uint64_t)&thread->arch.syscall + sizeof(struct x86_64_syscall_frame));
	uint64_t rsp0 = (uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame);
	thread->processor->arch.tss.rsp0 = rsp0;

	// thread->processor->arch.tss.esp0 =
	// ((uint64_t)&thread->arch.exception + sizeof(struct x86_64_exception_frame));

	/* restore segment bases for new thread */
	x86_64_wrmsr(
	  X86_MSR_FS_BASE, thread->arch.fs & 0xFFFFFFFF, (thread->arch.fs >> 32) & 0xFFFFFFFF);
	x86_64_wrmsr(
	  X86_MSR_KERNEL_GS_BASE, thread->arch.gs & 0xFFFFFFFF, (thread->arch.gs >> 32) & 0xFFFFFFFF);

	if(old) {
		asm volatile("xsave (%0)" ::"r"(old->arch.xsave_region), "a"(7), "d"(0) : "memory");
	}

	asm volatile("xrstor (%0)" ::"r"(thread->arch.xsave_region), "a"(7), "d"(0) : "memory");
	if((!old || old->ctx != thread->ctx) && thread->ctx) {
		arch_mm_switch_context(thread->ctx);
	}
	if((!old || old->active_sc != thread->active_sc) && thread->active_sc) {
		x86_64_secctx_switch(thread->active_sc);
		thread->processor->stats.sctx_switch++;
	}
	if(timeout) {
		clksrc_set_interrupt_countdown(timeout, false);
	}
	if(thread->arch.was_syscall) {
		x86_64_resume_userspace(&thread->arch.syscall);
	} else {
		x86_64_resume_userspace_interrupt(&thread->arch.exception);
	}
}
