/* This linker script was copied from the freebsd linker script
 * defaults for executables. It has been heavily modified. */
OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* We explicitly list program headers to that we can control
 * a) how many there are, and b) where they'll end up. If we
 * don't do this, the linker likes to assume large pages and
 * put things in the wrong place. */
PHDRS
{
	headers PT_PHDR PHDRS ;
	text PT_LOAD FILEHDR PHDRS ;
	dynamic PT_DYNAMIC;
}

SECTIONS
{
	/* Link at a high address for core. We'll need to put this much higher later. */
	. = 0x1000 + SIZEOF_HEADERS;
	/* Read-only sections, merged into text segment. */
	.text :
	{
		*(.text.start *.text .stub .text.* .gnu.linkonce.t.*)
		KEEP (*(.text.*personality*))
		/* .gnu.warning sections are handled specially by elf32.em.  */
	} :text =0x90909090
	.init :
	{
		KEEP (*(.init))
	} :text =0x90909090
	.fini :
	{
		KEEP (*(.fini))
	} :text =0x90909090
	PROVIDE (__etext = .);
	PROVIDE (_etext = .);
	PROVIDE (etext = .);
	.rodata :
	{
		KEEP(*(.rodata .rodata.* .gnu.linkonce.r.*))
	} :text
	.preinit_array :
	{
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	} :text
	.init_array :
	{
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT(.init_array.*)))
		KEEP (*(.init_array))
		PROVIDE_HIDDEN (__init_array_end = .);
	} :text
	.fini_array :
	{
		PROVIDE_HIDDEN (__fini_array_start = .);
		KEEP (*(SORT(.fini_array.*)))
		KEEP (*(.fini_array))
		PROVIDE_HIDDEN (__fini_array_end = .);
	} :text
	
	/* read-write sections, merged into data segment.
	 * Move up by 1 object slot size, and give us space
	 * for the null page. */
	/*. = ALIGN(0x40000000);
	. += 0x1000;
	*/
	.dynamic : { *(.dynamic) } :text :dynamic
	.ctors :
	{
		/* gcc uses crtbegin.o to find the start of
		the constructors, so we make sure it is
		first.  Because this is a wildcard, it
		doesn't matter if the user does not
		actually link against crtbegin.o; the
		linker won't look for a file to match a
		wildcard.  The wildcard also means that it
		doesn't matter which directory crtbegin.o
		is in.  */
		KEEP (*crtbegin.o(.ctors))
		KEEP (*crtbegin?.o(.ctors))
		/* We don't want to include the .ctor section from
		the crtend.o file until after the sorted ctors.
		The .ctor section from the crtend file contains the
		end of ctors marker and it must be last */
		KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
		KEEP (*(SORT(.ctors.*)))
		KEEP (*(.ctors))
	} :text
	.dtors :
	{
		KEEP (*crtbegin.o(.dtors))
		KEEP (*crtbegin?.o(.dtors))
		KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
		KEEP (*(SORT(.dtors.*)))
		KEEP (*(.dtors))
	} :text
	.data :
	{
		*(.data .data.* .gnu.linkonce.d.*)
		KEEP (*(.gnu.linkonce.d.*personality*))
		SORT(CONSTRUCTORS)
	} :text
	_edata = .; PROVIDE (edata = .);
	__bss_start = .;
	.bss :
	{
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		/* Align here to ensure that the .bss section occupies space up to
		_end.  Align after .bss to ensure correct alignment even if the
		.bss section disappears because there are no input sections.
		FIXME: Why do we need it? When there is no .bss section, we don't
		pad the .data section.  */
	} :text
	_end = .; PROVIDE (end = .);
	/DISCARD/ : { *(.note.GNU-stack) }
}

