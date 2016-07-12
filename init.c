struct sbi_device_msg {
	unsigned long dev;
	unsigned long cmd;
	unsigned long data;
	unsigned long private;
};
extern unsigned long va_offset;
#define __riscv_va_to_pa(x) ((void *)((uintptr_t)x - va_offset))

long (*sbi_send_device_msg)(struct sbi_device_msg *msg) = (long (*)(struct sbi_device_msg *))(-1984);

void arch_debug_putchar(unsigned char c)
{
	struct sbi_device_msg msg = {.dev = 1, .cmd = 1, .data = c, .private = 0 };
	sbi_send_device_msg(__riscv_va_to_pa(&msg));
	if(c == '\n')
		arch_debug_putchar('\r');
}

void debug_puts(char *s)
{
	while(*s)
		arch_debug_putchar(*s++);
}

void kernel_init(void)
{
	debug_puts("Hello, World!\n");
	for(;;);
}

