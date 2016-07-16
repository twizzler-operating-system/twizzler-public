#include <stdarg.h>
#include <debug.h>
_Noreturn void __panic(const char *file, int linenr, int flags, const char *msg, ...)
{
	(void)flags;
	va_list args;
	va_start(args, msg);
	printk("panic [%s:%d] - ", file, linenr);
	vprintk(msg, args);
	printk("\n");

	/* TODO */
	//if(flags & PANIC_UNWIND)
	//	debug_unwind();
	for(;;);
}

