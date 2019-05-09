.global _start


_start:
	movq $2, %rax
	movq $message, %rdi
	movq $12, %rsi
	syscall
	jmp .

message:
	.ascii  "Hello, world"

