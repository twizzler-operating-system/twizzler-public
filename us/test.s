.global _start
_start:
	xorq %rax, %rax
	syscall
	jmp .

