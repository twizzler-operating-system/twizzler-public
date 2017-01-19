
void serial_init();

void x86_64_init(void)
{
	serial_init();

	kernel_main();
}

