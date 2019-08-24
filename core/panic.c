#include <debug.h>
#include <interrupt.h>
#include <processor.h>
#include <stdarg.h>

static struct spinlock panic_lock = SPINLOCK_INIT;

void __panic(const char *file, int linenr, int flags, const char *msg, ...)
{
	spinlock_acquire(&panic_lock);
	/* TODO (minor): stop processors, cli */
	// processor_send_ipi(PROCESSOR_IPI_DEST_OTHERS, PROCESSOR_IPI_HALT, NULL, 0);
	arch_interrupt_set(false);
	va_list args;
	va_start(args, msg);
	printk("panic cpu %d thr %ld [%s:%d] - ",
	  current_processor ? current_processor->id : 0,
	  current_thread ? (long)current_thread->id : -1,
	  file,
	  linenr);
	vprintk(msg, args);
	printk("\n");

	if(flags & PANIC_UNWIND)
		debug_print_backtrace();
	// kernel_debug_entry();
	if(!(flags & PANIC_CONTINUE))
		for(;;)
			;
}
