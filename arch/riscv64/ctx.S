.section .text

.extern _gp

.macro RESTORE_ALL
	ld s1, (8*33)(sp)
	csrw sepc, s1
	ld s1, 0(sp)
	csrw sstatus, s1
	ld x1, (8*1)(sp)
	ld x3, (8*3)(sp)
	ld x4, (8*4)(sp)
	ld x5, (8*5)(sp)
	ld x6, (8*6)(sp)
	ld x7, (8*7)(sp)
	ld x8, (8*8)(sp)
	ld x9, (8*9)(sp)
	ld x10, (8*10)(sp)
	ld x11, (8*11)(sp)
	ld x12, (8*12)(sp)
	ld x13, (8*13)(sp)
	ld x14, (8*14)(sp)
	ld x15, (8*15)(sp)
	ld x16, (8*16)(sp)
	ld x17, (8*17)(sp)
	ld x18, (8*18)(sp)
	ld x19, (8*19)(sp)
	ld x20, (8*20)(sp)
	ld x21, (8*21)(sp)
	ld x22, (8*22)(sp)
	ld x23, (8*23)(sp)
	ld x24, (8*24)(sp)
	ld x25, (8*25)(sp)
	ld x26, (8*26)(sp)
	ld x27, (8*27)(sp)
	ld x28, (8*28)(sp)
	ld x29, (8*29)(sp)
	ld x30, (8*30)(sp)
	ld x31, (8*31)(sp)
	addi sp, sp, 8*37
.endm

.global riscv_switch_thread
/* void riscv_switch_thread(void *new_stack, void **old_sp); */
riscv_switch_thread:
	sd s0, -(8*1)(sp)
	sd s1, -(8*2)(sp)
	sd s2, -(8*3)(sp)
	sd s3, -(8*4)(sp)
	sd s4, -(8*5)(sp)
	sd s5, -(8*6)(sp)
	sd s6, -(8*7)(sp)
	sd s7, -(8*8)(sp)
	sd s8, -(8*9)(sp)
	sd s9, -(8*10)(sp)
	sd s10, -(8*11)(sp)
	sd s11, -(8*12)(sp)
	sd ra, -(8*13)(sp)

	sd sp, 0(a1)
	mv sp, a0
	
	ld s0, -(8*1)(sp)
	ld s1, -(8*2)(sp)
	ld s2, -(8*3)(sp)
	ld s3, -(8*4)(sp)
	ld s4, -(8*5)(sp)
	ld s5, -(8*6)(sp)
	ld s6, -(8*7)(sp)
	ld s7, -(8*8)(sp)
	ld s8, -(8*9)(sp)
	ld s9, -(8*10)(sp)
	ld s10, -(8*11)(sp)
	ld s11, -(8*12)(sp)
	ld ra, -(8*13)(sp)

	ret
.global riscv_switch_thread1
/* void riscv_switch_thread(void *new_stack, void **old_sp); */
riscv_switch_thread1:
	sd s0, -(8*1)(sp)
	sd s1, -(8*2)(sp)
	sd s2, -(8*3)(sp)
	sd s3, -(8*4)(sp)
	sd s4, -(8*5)(sp)
	sd s5, -(8*6)(sp)
	sd s6, -(8*7)(sp)
	sd s7, -(8*8)(sp)
	sd s8, -(8*9)(sp)
	sd s9, -(8*10)(sp)
	sd s10, -(8*11)(sp)
	sd s11, -(8*12)(sp)
	sd ra, -(8*13)(sp)

	sd sp, 0(a1)
	mv sp, a0
	
	ld s0, -(8*1)(sp)
	ld s1, -(8*2)(sp)
	ld s2, -(8*3)(sp)
	ld s3, -(8*4)(sp)
	ld s4, -(8*5)(sp)
	ld s5, -(8*6)(sp)
	ld s6, -(8*7)(sp)
	ld s7, -(8*8)(sp)
	ld s8, -(8*9)(sp)
	ld s9, -(8*10)(sp)
	ld s10, -(8*11)(sp)
	ld s11, -(8*12)(sp)
	ld ra, -(8*13)(sp)

	ret


_new_context_entry:
	mv a0, s1
	mv ra, s0
	ret

/* void riscv_new_context(void *top, void **sp, void *jump, void *arg); */
.global riscv_new_context
riscv_new_context:
	sd a0, 0(a1)
	sd a2, -8(a0)
	sd a3, -16(a0)
	la t0, _new_context_entry
	sd t0, -(8*13)(a0)

	ret

.extern arch_interrupt_entry
.global riscv_exception_entry
/* TODO: set sscratch during kernel init */
riscv_exception_entry:
	j .
	csrrw sp, sscratch, sp
	bnez sp, _save_state
	csrrw sp, sscratch, sp
_save_state:
	addi sp, sp, -(8*37)
	sd x1, (8*1)(sp)
	//skip x2 (sp)
	sd x3, (8*3)(sp)
	sd x4, (8*4)(sp)
	sd x5, (8*5)(sp)
	sd x6, (8*6)(sp)
	sd x7, (8*7)(sp)
	sd x8, (8*8)(sp)
	sd x9, (8*9)(sp)
	sd x10, (8*10)(sp)
	sd x11, (8*11)(sp)
	sd x12, (8*12)(sp)
	sd x13, (8*13)(sp)
	sd x14, (8*14)(sp)
	sd x15, (8*15)(sp)
	sd x16, (8*16)(sp)
	sd x17, (8*17)(sp)
	sd x18, (8*18)(sp)
	sd x19, (8*19)(sp)
	sd x20, (8*20)(sp)
	sd x21, (8*21)(sp)
	sd x22, (8*22)(sp)
	sd x23, (8*23)(sp)
	sd x24, (8*24)(sp)
	sd x25, (8*25)(sp)
	sd x26, (8*26)(sp)
	sd x27, (8*27)(sp)
	sd x28, (8*28)(sp)
	sd x29, (8*29)(sp)
	sd x30, (8*30)(sp)
	sd x31, (8*31)(sp)

	csrrw s0, sscratch, x0
	
	csrr s1, scause
	sd s1, (8*32)(sp)
	csrr s2, sepc
	li t0, 8
	bne t0, s1, _write_sepc
	addi s2, s2, 4 //advance instruction pointer for syscall
_write_sepc:
	sd s2, (8*33)(sp)
	csrr s1, sbadaddr
	sd s1, (8*34)(sp)
	csrr s1, sstatus
	sd s1, 0(sp)
	csrr s1, sip
	sd s1, (8*35)(sp)

1:  auipc gp, %pcrel_hi(_gp)
	addi gp, gp, %pcrel_lo(1b)
	
	mv a0, sp
	li a1, 0x12345
	sd a1, (8*2)(sp)
	call arch_interrupt_entry

	beqz s0, _ret_to_kernel

	csrw sscratch, s0
	RESTORE_ALL
	csrrw sp, sscratch, sp
	sret

_ret_to_kernel:
	RESTORE_ALL
	sret

