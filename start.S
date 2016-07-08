#include <thread.h>
.global _start
.extern kernel_init
_start:
1:
	/* set global pointer */
	auipc gp, %pcrel_hi(_gp)
	addi gp, gp, %pcrel_lo(1b)

	/* clear bss */
	la a0, __kernel_bss_start
	mv a1, x0
	la a2, __kernel_bss_end
	addi a2, a2, -8
	bssclear:
		addi a0, a0, 8
		sd a1, 0(a0)
		bne a0, a2, bssclear

	/* set initial stack pointer and thread pointer */
	la sp, initial_boot_stack + KERNEL_STACK_SIZE
	mv tp, x0

	/* disable FPU */
	li t0, 0x00003000
	csrc sstatus, t0
	
	/* determine the VA to PA offset */
	mv a0, x0
	la a1, meminfo
	jalr x0,-2016
	ld t0, base
	ld t1, offset
	sub t1, t1, t0
	la s0, va_offset
	sd t1, 0(s0)

	/* done, ready for C code */
	tail kernel_init
	j .

.section .data
.global va_offset
va_offset: .space 8
meminfo:
	base: .space 8
	size: .space 8
	node_id: .space 8

offset:  .quad 0xFFFFFFFF80000000 //TODO: make global define

.section .bss
.align 12
initial_boot_stack: .space KERNEL_STACK_SIZE
_gp: .space 8

