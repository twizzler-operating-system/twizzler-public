#include <thread-bits.h>
#include <machine/memory.h>
.global _start
.extern _init
.extern kernel_early_init
.extern riscv_exception_entry
_start:
1:
	/* set global pointer */
	auipc gp, %pcrel_hi(_gp)
	addi gp, gp, %pcrel_lo(1b)
	/* clear bss */
	la a0, __kernel_bss_start
	la a2, __kernel_bss_end
	addi a2, a2, -8
	bssclear:
		addi a0, a0, 8
		sd x0, 0(a0)
		ble a0, a2, bssclear
	/* set initial stack pointer and thread pointer */
	la sp, initial_boot_stack + KERNEL_STACK_SIZE
	mv tp, x0

	/* disable FPU */
	li t0, 0x00003000
	csrc sstatus, t0
	
	/* determine the VA to PA offset */
	mv a0, x0
	la a1, riscv_meminfo
	jalr x0,-2016
	ld t0, base
	li t1, KERNEL_VIRTUAL_BASE
	sub t1, t1, t0
	la s0, va_offset
	sd t1, 0(s0)

	call _init_page_table

	/* initialize trap handler address */
	la t0, riscv_exception_entry
	csrw stvec, t0

	/* done, ready for C code */
	call kernel_early_init

	call _init

	call kernel_main
	j .


_init_page_table:
	csrr t0, sptbr
	la t1, start_phys_free
	li t2, 0x1000
	add t2, t2, t0
	sd t2, 0(t1)
	ld t1, va_offset
	add t0, t0, t1
	li t1, ((PHYSICAL_MAP_START >> 30) & 0x1FF) * 8
	add t0, t0, t1

	li t3, 0x10000000 >> 2
	li t4, 128
	li t2, 0xF

	_loop:
		sd t2, 0(t0)
		addi t0, t0, 8
		add t2, t2, t3
		addi t4, t4, -1
	bne t4, x0, _loop
	
	ret


.section .bss
initial_boot_stack: .space KERNEL_STACK_SIZE
.global va_offset
va_offset: .space 8
.global start_phys_free
start_phys_free: .space 8
.global riscv_meminfo
riscv_meminfo:
	base: .space 8
	size: .space 8
	node_id: .space 8

.align 12
.global _gp
_gp: .space 8

