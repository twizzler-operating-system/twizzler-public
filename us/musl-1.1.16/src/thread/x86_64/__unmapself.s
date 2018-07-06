/* Copyright 2011-2012 Nicholas J. Kain, licensed under standard MIT license */
.text
.global __unmapself
.type   __unmapself,@function
.extern __twix_syscall_target
__unmapself:
	movl $11,%eax   /* SYS_munmap */
	call __twix_syscall_target
	xor %rdi,%rdi   /* exit() args: always return success */
	movl $60,%eax   /* SYS_exit */
	call __twix_syscall_target
