.global __vfork
.weak vfork
.type __vfork,@function
.type vfork,@function
__vfork:
vfork:
	pop %rdx
	mov $58,%eax
	mov $0x3ffec0001400, %rcx
	call *%rcx
	push %rdx
	mov %rax,%rdi
	jmp __syscall_ret
