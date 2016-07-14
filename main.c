void _init(void);
void kernel_main(void)
{
	_init();
	printk("Kernel main!\n");

	for(;;);
}

