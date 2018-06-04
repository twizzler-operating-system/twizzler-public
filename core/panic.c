#include <stdarg.h>
#include <debug.h>
#include <interrupt.h>
_Noreturn void __panic(const char *file, int linenr, int flags, const char *msg, ...)
{
	/* TODO (dbittman): stop processors, cli */
	arch_interrupt_set(false);
	va_list args;
	va_start(args, msg);
	printk("panic [%s:%d] - ", file, linenr);
	vprintk(msg, args);
	printk("\n");

	if(flags & PANIC_UNWIND)
		debug_print_backtrace();
	kernel_debug_entry();
	for(;;);
}

