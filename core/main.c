#include <debug.h>
#include <memory.h>
#include <guard.h>

void func()
{
	printk("Func!\n");
}

void tes2t(void)
{
	defer(func);
}

/* functions called from here expect virtual memory to be set up. However, functions
 * called from here cannot rely on global contructors having run, as those are allowed
 * to use memory management routines, so they are run after this.
 */
void kernel_early_init(void)
{
	mm_init();
}

/* at this point, memory management, interrupt routines, global constructors, and shared
 * kernel state between nodes have been initialized. Now initialize all application processors
 * and per-node threading.
 */
void kernel_main(void)
{
	printk("Kernel main!\n");

	tes2t();
	panic("init completed");
	for(;;);
}

