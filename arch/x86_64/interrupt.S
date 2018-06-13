# x86 exceptions can either push an error code, or not. Here
# we create two macros for entry points for exceptions, and
# we make it so that exceptions that don't push an errorcode
# have a dummy code pushed so that the exception frames are
# the same for both types.

#include <thread-bits.h>
.macro isr_errcode num, handler
.global x86_64_isr\num
.align 8
x86_64_isr\num:
	pushq $\num # push the interrupt number so that the C code knows what to do
	jmp \handler
.endm

.macro isr_noerrcode num, handler
.global x86_64_isr\num
.align 8
x86_64_isr\num:
	pushq $0
	pushq $\num
	jmp \handler
.endm

# these exceptions push error codes
.irp num,8,10,11,12,13,14,17
isr_errcode \num, isr_common
.endr

.irp num,0,1,2,3,4,5,6,7,9,15,16,18,19,20,21,22,23,24,25,26,27,28,29,30,31
isr_noerrcode \num, isr_common
.endr
# these are split into two so it's easier to read
.irp num,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59
isr_noerrcode \num, isr_common
.endr

.irp num,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80
isr_noerrcode \num, isr_common
.endr

.global x86_64_isr_halt
x86_64_isr_halt:
	cli
	call x86_64_ipi_halt
	.halt: hlt
	jmp .halt

.global x86_64_isr_ignore
x86_64_isr_ignore:
	pushq $0
	pushq $0x80
	jmp ignore_handler

.macro PUSH_CONTEXT
# save the frame
	pushq %rdi
	pushq %rsi
	pushq %rdx
	pushq %rcx
	pushq %rax
	pushq %r8
	pushq %r9
	pushq %r10
	pushq %r11
	pushq %rbx
	pushq %rbp
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15
.endm

.macro RESTORE_CONTEXT
	# restore the frame
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbp
	popq %rbx
	popq %r11
	popq %r10
	popq %r9
	popq %r8
	popq %rax
	popq %rcx
	popq %rdx
	popq %rsi
	popq %rdi
	
	add $16, %rsp # cleanup error code and int number
.endm


.global x86_64_isr_shootdown
.extern x86_64_ipi_tlb_shootdown
x86_64_isr_shootdown:
	pushq $0
	pushq $0x80
	PUSH_CONTEXT
	call x86_64_ipi_tlb_shootdown
	RESTORE_CONTEXT
	iretq

.extern x86_64_signal_eoi
ignore_handler:
	PUSH_CONTEXT
	call x86_64_signal_eoi
	RESTORE_CONTEXT
	iretq


.extern x86_64_exception_entry
isr_common:
	cld
	PUSH_CONTEXT
	movq %rsp, %rdi
	testl $3, 144(%rsp) #test CPL for pushed CS desc
	je kernel_exception

	swapgs
	movq $1, %rsi
	movq %gs:16, %rsp
	addq $KERNEL_STACK_SIZE, %rsp

	call x86_64_exception_entry


kernel_exception:
	movq $0, %rsi
	call x86_64_exception_entry
	RESTORE_CONTEXT
	iretq


.global x86_64_resume_userspace_interrupt
/* _Noreturn void x86_64_resume_userspace_interrupt(void *frame); */
x86_64_resume_userspace_interrupt:
	movq %rdi, %rsp
	RESTORE_CONTEXT
	swapgs
	iretq

