#include <machine/pc-multiboot.h>
#include <machine/memory.h>
#include <debug.h>
void serial_init();

extern void _init();
void x86_64_init(struct multiboot *mth)
{
	serial_init();

	if(!(mth->flags & MULTIBOOT_FLAG_MEM))
		panic("don't know how to detect memory!");
	printk("%lx\n", mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET);

	for(;;);

	kernel_early_init();
	_init();
	kernel_main();
}

