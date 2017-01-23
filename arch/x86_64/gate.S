#include <thread-bits.h>
.global x86_64_syscall_entry_from_userspace
x86_64_syscall_entry_from_userspace:
	xchg %bx, %bx
	swapgs
	movq %rsp, %gs:0
	movq %gs:8, %rsp

	pushq $0x1B      // user CS
	pushq %gs:0
	pushq %rcx
	pushq %r11
	pushq %rax
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %r8
	pushq %r9
	pushq %r10

	movq %rsp, %rdi
	movq %gs:16, %rsp
	addq $KERNEL_STACK_SIZE, %rsp

	movq $0, %r8
	call x86_64_syscall_entry
	movq $0x4321, %rax


.global x86_64_resume_userspace
/* _Noreturn void x86_64_resume_userspace(void *tcb); */
x86_64_resume_userspace:
	xchg %bx, %bx
	cli
	movq %rdi, %rsp

	popq %r10
	popq %r9
	popq %r8
	popq %rdx
	popq %rsi
	popq %rdi
	popq %rax
	popq %r11
	popq %rcx
	popq %rsp
	

	/* TODO: check if rcx is canonical */
	bts $9, %r11 // set IF in EFLAGS
	//and $~(0x200), %r11 // set IF in EFLAGS
	

	swapgs
	sysretq

