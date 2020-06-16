.global __vfork
.weak vfork
.type __vfork,@function
.type vfork,@function
.extern __twix_syscall_target
__vfork:
vfork:
	jmp fork
	#pop %rdx
	#mov $58,%eax
	#call __twix_syscall_target
	#push %rdx
	#mov %rax,%rdi
	#jmp __syscall_ret
