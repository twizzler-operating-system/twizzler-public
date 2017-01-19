#include <machine/memory.h>
#include <thread-bits.h>
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002

/* The flags for the Multiboot header. */
#define MULTIBOOT_HEADER_FLAGS 0x00010002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* the kernel is linked to the virtual addresses. But we start off with paging disabled,
 * which means we need to translate all addresses until we enable paging and switch to long
 * mode. This is a pain, but it gives us the ability to simply the C code quite a bit by
 * enabling paging as early as possible (plus, we need to switch to long mode, which requires
 * paging anyway... and we want the C code to be all uniform in 64-bit.)
 */
#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)

.section .boot
.code32
.global _start

.align 8

.extern kernel_data_end
.extern kernel_bss_end

.type multiboot_header,STT_OBJECT
multiboot_header:
    /* magic */
    .int MULTIBOOT_HEADER_MAGIC
    /* flags */
    .int MULTIBOOT_HEADER_FLAGS
    /* checksum */
    .int -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    /* header_addr */
    .int PHYS(multiboot_header)
    /* load_addr */
    .int PHYS(_start)
    /* load_end_addr */
    .int PHYS(kernel_data_end)
    /* bss_end_addr */
    .int PHYS(kernel_bss_end)
    /* entry_addr */
    .int PHYS(_start)





_start:
	movl $PHYS(kernel_bss_start), %edi
    movl $PHYS(kernel_bss_end), %ecx
    subl %edi, %ecx
    shrl $2, %ecx
	.Lbss:
	    movl $0, (%edi)
    	addl $4, %edi
	    loop .Lbss


init_paging:
	/* 1 - 1 mapping for lower */
	movl $PHYS(boot_pdpt), %eax
	orl $3, %eax
	movl %eax, PHYS(boot_pml4)

	/* upper -2GB mapping */
	movl $PHYS(boot_pdpt_high), %eax
	orl $3, %eax
	movl %eax, PHYS(boot_pml4 + 8*511)

	movl $PHYS(boot_pd), %eax
	orl $3, %eax
	movl %eax, PHYS(boot_pdpt)

	movl $PHYS(boot_pd), %eax
    orl  $3, %eax
    movl %eax, PHYS(boot_pdpt_high + 8*510)
	
	mov $PHYS(boot_pd), %edi
	mov $0x200, %ecx
	xor %eax, %eax
	.Lfill_pd1:
		movl %eax, %ebx
		shll $21, %ebx
		orl $0x83, %ebx
		movl %ebx, (%edi)
		addl $8, %edi
		inc %eax
		loop .Lfill_pd1

	mov %cr4, %eax
    btsl $(5), %eax
    mov %eax, %cr4

	movl $PHYS(boot_pml4), %eax
    mov %eax, %cr3

	movl $0xC0000080, %ecx
    rdmsr
    bts $8, %eax
    bts $11, %eax //NX
    wrmsr

	mov %cr0,  %eax
    btsl $(31), %eax
    mov %eax,  %cr0

	lgdt (PHYS(gdtdesc))
	ljmp $0x08,$PHYS(_start_64_low)

.section .text
.code64

_start_64_low:
	movq $_start_64, %rax
	jmpq *%rax


.extern x86_64_init
_start_64:
	lgdt (gdtdesc_virt)
	movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %ss
    movw %ax, %gs
    movw %ax, %ss

	mov  $(initial_boot_stack + KERNEL_STACK_SIZE), %rsp

	call x86_64_init

	jmp . // hang

.section .bss
.align KERNEL_STACK_SIZE
.global initial_boot_stack
initial_boot_stack:
.skip KERNEL_STACK_SIZE
__stack_top:

.align 0x1000
boot_pml4:
	.skip 0x1000

boot_pdpt:
	.skip 0x1000

boot_pdpt_high:
	.skip 0x1000

boot_pd:
	.skip 0x1000


.section .data
.align 8
gdtdesc:
    .word      0x47
    .long      PHYS(gdtable)
    .long      0

.align 8
gdtdesc_virt:
    .word      0x47
    .quad      gdtable

.align 8
gdtable:
    .word      0, 0                    # null segment
    .byte      0, 0, 0, 0

    .word      0xFFFF, 0               # 64-bit kernel code segment
    .byte      0, 0x9A, 0xAF, 0

    .word      0xFFFF, 0               # 64-bit kernel data segment
    .byte      0, 0x92, 0xAF, 0

	.word      0xFFFF, 0               # 64-bit user code segment
    .byte      0, 0xFA, 0xAF, 0

    .word      0xFFFF, 0               # 64-bit user data segment
    .byte      0, 0xF2, 0xAF, 0

	.quad      0 # For tss entry
	.quad      0

