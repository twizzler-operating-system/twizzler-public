#include <machine/memory.h>
#include <thread-bits.h>
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002

/* The flags for the Multiboot header. */
#define MULTIBOOT_HEADER_FLAGS 0x00010002
#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define PHYS_LOAD_ADDRESS (KERNEL_PHYSICAL_BASE + KERNEL_LOAD_OFFSET)
#define PHYS_ADDR_DELTA (KERNEL_VIRTUAL_BASE + KERNEL_LOAD_OFFSET - PHYS_LOAD_ADDRESS)
#define PHYS(x) ((x) - PHYS_ADDR_DELTA)


.section .boot
.code32
.global _start
_start:
	mov $0x1234, %eax
    jmp real_start

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
    .int PHYS(real_start)





real_start:
	mov $0x5678, %eax
	jmp .

.section .bss
.align KERNEL_STACK_SIZE * 16
.global initial_boot_stack
initial_boot_stack:
.skip KERNEL_STACK_SIZE
__stack_top:

