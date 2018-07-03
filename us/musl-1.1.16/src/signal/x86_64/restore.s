	nop
.global __restore_rt
.type __restore_rt,@function
__restore_rt:
	mov $15, %rax
	mov $0x3ffec0001400, %rcx
	call *%rcx
.size __restore_rt,.-__restore_rt
