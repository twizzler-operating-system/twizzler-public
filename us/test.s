.global _start
_start:
	xorq %rax, %rax
	syscall
	push %rax
	syscall
	jmp .

