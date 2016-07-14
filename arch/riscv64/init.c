struct sbi_device_msg {
	unsigned long dev;
	unsigned long cmd;
	unsigned long data;
	unsigned long private;
};
#include <memory.h>
extern unsigned long va_offset;
static inline void *__riscv_va_to_pa(void *x)
{
	uintptr_t t = (uintptr_t)x;
	if(t >= 0xFFFFFFFF80000000)
		return (void *)(t - va_offset);
	return (void *)(t - PHYSICAL_MAP_START);
}
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

char _st[4024];
void *sp, *csp;
void riscv_switch_thread(void *new_stack, void **old_sp);
void riscv_switch_thread1(void *new_stack, void **old_sp);

void test(void *arg)
{
	printk("Test %p %lx %p!\n", sp, *((uint64_t *)sp - 13), ((uint64_t *)sp - 13));

	riscv_switch_thread1(sp, &csp);
	for(;;);
}

void usermode(void)
{
	asm volatile("scall; j .");
	for(;;);
}

typedef void (*func_ptr)(void);
extern func_ptr __init_array_start, __init_array_end;
void _init(void)
{
        /* _init is called by main (which, if you know about how a C program usually
         * initializes) is pretty funny. But here, we want to be able to allocate
         * memory inside constructor functions, so we need to wait until we have a memory
         * manager.
         *
         * Anyway. This function calls constructor functions. This is handled with a little
         * bit of linker magic - we let the linker script tell us where this section is
         * so that we can iterate over the array and call the functions.
         */
        for ( func_ptr* func = &__init_array_start; func != &__init_array_end; func++ ) {
                (*func)();
        }
}

void riscv_new_context(void *top, void **sp, void *jump, void *arg);
void kernel_init(void)
{
	riscv_new_context(_st + 4024, &csp, test, NULL);
	printk("Switch!\n");
	riscv_switch_thread(csp, &sp);
	printk("And back!\n");

	_init();
	for(;;);
	asm(
			"li t0, (1 << 1) | (1 << 5);"
			"li t1, 1;"
			"li t2, 1 << 1;"
			"csrw sie, t0;"
			"csrc sip, t0;"
			//"csrs sip, t2;"
			"csrs sstatus, t1;"
		::: "t0", "t1");

	


	for(;;);
	void *u = (void *)(PHYSICAL_MAP_START + 0x4000000);
	memcpy(u, (void *)&usermode, 128);
	asm(
			"csrw sepc, %0;"
			"mv sp, %1;"
			"sret;"
			::"r"(u), "r"(PHYSICAL_MAP_START + 0x4000000 + 0x1000): "t0");

	for(;;);
	for(;;) printk("Hello!\n");
}

