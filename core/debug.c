#include <trace.h>
#include <interrupt.h>
#include <thread.h>
#include <processor.h>

int trace_indent_level = 0;

/* TODO: clean up */
int serial_received();
void kernel_debug_entry(void)
{
	while(!serial_received());

	arch_processor_reset();
}

