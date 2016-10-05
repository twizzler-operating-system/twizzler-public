#include <interrupt.h>
#include <thread.h>
void kernel_syscall_entry(struct interrupt_frame *frame)
{
	int oldec = atomic_fetch_or(&current_thread->econtext, ECONTEXT_KERNEL);
	current_thread->econtext = oldec;
}

