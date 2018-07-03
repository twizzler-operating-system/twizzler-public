/* Copyright 2011-2012 Nicholas J. Kain, licensed under standard MIT license */
.text
.global __unmapself
.type   __unmapself,@function
__unmapself:
	movl $11,%eax   /* SYS_munmap */
	mov $0x3ffec0001400, %rcx
	call *%rcx
	xor %rdi,%rdi   /* exit() args: always return success */
	movl $60,%eax   /* SYS_exit */
	mov $0x3ffec0001400, %rcx
	call *%rcx
