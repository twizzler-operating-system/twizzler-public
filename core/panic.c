#include <stdarg.h>
#include <debug.h>
_Noreturn void __panic(const char *file, int linenr, int flags, const char *msg, ...)
{
	/* TODO (dbittman): stop processors, cli */
	va_list args;
	va_start(args, msg);
	printk("panic [%s:%d] - ", file, linenr);
	vprintk(msg, args);
	printk("\n");

	if(flags & PANIC_UNWIND)
		debug_print_backtrace();
	for(;;);
}

