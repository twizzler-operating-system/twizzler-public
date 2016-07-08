ENTRY(_start)
OUTPUT_FORMAT("elf64-littleriscv")
SECTIONS
{
	. = 0xFFFFFFFF80000000;
	kernel_start = .;
	. += SIZEOF_HEADERS;
	.text : AT(0x0) {
		*(.text*)
	}

	.data : ALIGN(0x1000) {
		*(.data*)
	}

	.rodata : ALIGN(0x1000) {
		*(.rodata*)
	}

	.bss : ALIGN(0x1000) {
		__kernel_bss_start = .;
		*(.bss*)
		__kernel_bss_end = .;
	}
	. = ALIGN(8);
}

