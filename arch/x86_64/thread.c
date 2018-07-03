#include <syscall.h>
#include <thread.h>
#include <processor.h>

void arch_thread_become(struct arch_syscall_become_args *ba)
{
	if(current_thread->arch.was_syscall) {
		current_thread->arch.syscall.rax = ba->rax;
		current_thread->arch.syscall.rbx = ba->rbx;
		/* note: rcx holds return RIP, so don't set it */
		current_thread->arch.syscall.rdx = ba->rdx;
		current_thread->arch.syscall.rdi = ba->rdi;
		current_thread->arch.syscall.rsi = ba->rsi;
		current_thread->arch.syscall.rbp = ba->rbp;
		current_thread->arch.syscall.rsp = ba->rsp;
		current_thread->arch.syscall.r8 = ba->r8;
		current_thread->arch.syscall.r9 = ba->r9;
		current_thread->arch.syscall.r10 = ba->r10;
		current_thread->arch.syscall.r11 = ba->r11;
		current_thread->arch.syscall.r12 = ba->r12;
		current_thread->arch.syscall.r13 = ba->r13;
		current_thread->arch.syscall.r14 = ba->r14;
		current_thread->arch.syscall.r15 = ba->r15;

		current_thread->arch.syscall.rcx = ba->target_rip;
	} else {
		current_thread->arch.exception.rax = ba->rax;
		current_thread->arch.exception.rbx = ba->rbx;
		current_thread->arch.exception.rcx = ba->rcx;
		current_thread->arch.exception.rdx = ba->rdx;
		current_thread->arch.exception.rdi = ba->rdi;
		current_thread->arch.exception.rsi = ba->rsi;
		current_thread->arch.exception.rbp = ba->rbp;
		current_thread->arch.exception.userrsp = ba->rsp;
		current_thread->arch.exception.r8 = ba->r8;
		current_thread->arch.exception.r9 = ba->r9;
		current_thread->arch.exception.r10 = ba->r10;
		current_thread->arch.exception.r11 = ba->r11;
		current_thread->arch.exception.r12 = ba->r12;
		current_thread->arch.exception.r13 = ba->r13;
		current_thread->arch.exception.r14 = ba->r14;
		current_thread->arch.exception.r15 = ba->r15;
		
		current_thread->arch.exception.rip = ba->target_rip;
	}
}

int arch_syscall_thrd_ctl(int op, long arg)
{
	switch(op) {
		case THRD_CTL_SET_FS:
			current_thread->arch.fs = arg;
			break;
		case THRD_CTL_SET_GS:
			current_thread->arch.gs = arg;
			break;
		default:
			return -1;
	}
	return 0;
}

