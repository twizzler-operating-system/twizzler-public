#include <thread-bits.h>
.global x86_64_syscall_entry_from_userspace
x86_64_syscall_entry_from_userspace:
	xchg %bx, %bx
	swapgs
	movq %rsp, %gs:0
	movq %gs:8, %rsp

	/* TODO: I don't think we need to save this many registers here. */
	pushq $0x1B      // user CS
	pushq %gs:0
	pushq %rbp
	pushq %rcx
	pushq %r11
	pushq %rax
	pushq %rbx
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	movq %rsp, %rdi
	movq %gs:16, %rsp
	addq $KERNEL_STACK_SIZE, %rsp

	call x86_64_syscall_entry


.global x86_64_resume_userspace
/* _Noreturn void x86_64_resume_userspace(void *tcb); */
x86_64_resume_userspace:
	movq %rdi, %rsp

	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %r10
	popq %r9
	popq %r8
	popq %rdx
	popq %rsi
	popq %rdi
	popq %rbx
	popq %rax
	popq %r11
	popq %rcx
	popq %rbp
	popq %rsp
	
	/* TODO: check if rcx is canonical */
	bts $9, %r11 // set IF in EFLAGS

	swapgs
	sysretq

