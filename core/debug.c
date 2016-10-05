#include <trace.h>
#include <interrupt.h>
#include <thread.h>

int trace_indent_level = 0;

void kernel_debug_entry(struct interrupt_frame *frame)
{
	int oldec = atomic_fetch_or(&current_thread->econtext, ECONTEXT_DEBUG);
	current_thread->econtext = oldec;
}

