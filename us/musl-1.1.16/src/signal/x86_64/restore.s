	nop
.global __restore_rt
.type __restore_rt,@function
.extern twix_syscall
.weak twix_syscall
__restore_rt:
	mov $15, %rdi
	call twix_syscall
.size __restore_rt,.-__restore_rt
