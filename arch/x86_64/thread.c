#include <processor.h>
#include <syscall.h>
#include <thread.h>

uintptr_t arch_thread_instruction_pointer(void)
{
	if(!current_thread)
		panic("cannot call %s before threading", __FUNCTION__);
	if(current_thread->arch.was_syscall) {
		return current_thread->arch.syscall.rcx;
	} else {
		return current_thread->arch.exception.rip;
	}
}

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
		case THRD_CTL_SET_IOPL:
			/* TODO (sec): permission check */
			current_thread->arch.syscall.r11 |= ((arg & 3) << 12);
			break;
		default:
			return -1;
	}
	return 0;
}

void arch_thread_raise_call(struct thread *t, void *addr, long a0, void *info, size_t infolen)
{
	/* TODO: sanity check stack address */
	if(t != current_thread) {
		panic("NI - raise fault in non-current thread");
	}

	uint64_t *arg0, *arg1, *jmp, *stack, *rsp, *rbp;

	if(t->arch.was_syscall) {
		stack = (uint64_t *)t->arch.syscall.rsp;
		arg0 = &t->arch.syscall.rdi;
		arg1 = &t->arch.syscall.rsi;
		jmp = &t->arch.syscall.rcx;
		rsp = &t->arch.syscall.rsp;
		rbp = &t->arch.syscall.rbp;
	} else {
		stack = (uint64_t *)t->arch.exception.userrsp;
		arg0 = &t->arch.exception.rdi;
		arg1 = &t->arch.exception.rsi;
		jmp = &t->arch.exception.rip;
		rsp = &t->arch.exception.userrsp;
		rbp = &t->arch.exception.rbp;
	}

	long red_zone = 128;

	uintptr_t stack_after = (uintptr_t)stack - (infolen + 5 * 8 + red_zone);
	uintptr_t bot_stack = ((uintptr_t)stack & ~(OBJ_MAXSIZE - 1)) + OBJ_NULLPAGE_SIZE;
	if(stack_after <= bot_stack) {
		printk("thread %ld exceeded stack during fault raise\n", t->id);
		thread_exit();
		return;
	}

	/* TODO: validate that stack is in a reasonable object */
	if(((unsigned long)stack & 0xFFFFFFFFFFFFFFF0) != (unsigned long)stack) {
		//	panic("NI");
		/* set up stack alignment correctly
		 * (mis-aligned going into a function call) */
		stack--;
	}

	stack -= red_zone / 8;

	*--stack = *jmp;
	*--stack = *rbp;
	// printk("raise: from %lx, orig frame = %lx\n", *jmp, *rbp);
	// printk("raise: Setting stack = %p = %lx\n", stack, *rbp);
	*rbp = (long)stack;
	// printk("raise: Setting rbp = %lx\n", *rbp);

	if(infolen & 0xf) {
		panic("infolen must be 16-byte aligned (was %ld)", infolen);
	}

	stack -= infolen / 8;
	long info_base_user = (long)stack;
	memcpy(stack, info, infolen);

	*--stack = *rsp;
	*--stack = *arg1;
	*--stack = *arg0;
	*jmp = (long)addr;
	*arg0 = a0;
	*arg1 = info_base_user;
	*rsp = (long)stack;
}
