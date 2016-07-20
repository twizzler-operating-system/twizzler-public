#include <debug.h>
#include <memory.h>
void _init(void);
void kernel_main(void)
{
	mm_init();
	_init();
	printk("Kernel main %llx!\n", __round_up_pow2(0x1001));

	panic("init completed");
	for(;;);
}

