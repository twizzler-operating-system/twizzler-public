#include <trace.h>
#include <interrupt.h>
#include <thread.h>
#include <processor.h>

int trace_indent_level = 0;

void kernel_debug_entry(void)
{
	arch_processor_reset();
}

