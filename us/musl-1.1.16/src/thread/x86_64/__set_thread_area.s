/* Copyright 2011-2012 Nicholas J. Kain, licensed under standard MIT license */
.text
.global __set_thread_area
.type __set_thread_area,@function
.extern __twix_syscall_target
__set_thread_area:
	mov %rdi,%rsi           /* shift for syscall */
	#movl $0x1002,%edi       /* SET_FS register */
	#movl $158,%eax          /* set fs segment to */
	movl $0x1, %edi
	movl $10, %eax
	syscall
	ret
	call __twix_syscall_target
