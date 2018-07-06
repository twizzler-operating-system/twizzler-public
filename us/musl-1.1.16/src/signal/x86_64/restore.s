	nop
.global __restore_rt
.type __restore_rt,@function
.extern __twix_syscall_target
__restore_rt:
	mov $15, %rax
	call __twix_syscall_target
.size __restore_rt,.-__restore_rt
