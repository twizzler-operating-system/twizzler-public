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

	la sp, initial_boot_stack + KERNEL_STACK_SIZE
	mv tp, x0

	/* disable FPU */
	li t0, 0x00003000
	csrc sstatus, t0
	
	tail kernel_init
	
	j .

	ld t0, htif
	mv a0, x0
	la a1, meminfo
	jalr x0,-2016

	ld t0, base

	ld t1, offset
	sub t1, t1, t0
	la s0, va_offset
	sd t1, 0(s0)

	la a0, devmsg
	sub a0, a0, t1

	jalr x0, -1984

	_loop:
	tail _loop


.section .data
_gp: .space 8
va_offset: .space 8
meminfo:
	base: .space 8
	size: .space 8
	node_id: .space 8

devmsg:
	dev: .quad 1
	cmd: .quad 1
	data: .quad 0x69
	priv: .quad 0

htif:    .quad 0xFFFFFFFF40000000
htif_pte: .quad 0xFFFFFC00000F
payload: .quad 0x0101000000000078
offset:  .quad 0xFFFFFFFF80000000

.section .bss
.align 12
initial_boot_stack: .space KERNEL_STACK_SIZE
