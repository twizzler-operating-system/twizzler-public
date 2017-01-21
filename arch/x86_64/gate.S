
.extern x86_64_syscall_entry
x86_64_syscall_entry:
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
	call x86_64_syscall_entry





/* _Noreturn void x86_64_resume_userspace(void *tcb); */
x86_64_resume_userspace:
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
	
	bts $9, %r11 // set IF in EFLAGS

	/* TODO: check if rcx is canonical */

	swapgs
	sysret

