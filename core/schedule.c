#include <processor.h>
#include <debug.h>

void thread_schedule_resume_proc(struct processor *proc)
{
	printk("resuming %p\n", proc);
	while(true) {
		if(current_thread && current_thread->state == THREADSTATE_RUNNING)
			linkedlist_insert(&proc->runqueue, &current_thread->entry, current_thread);
		struct thread *next = linkedlist_remove_tail(&proc->runqueue);
		if(next) {
			arch_thread_resume(next);
		} else {
			/* we're halting here, but the arch_processor_halt function will return
			 * after an interrupt is fired. Since we're in kernel-space, any interrupt
			 * we get will not invoke the scheduler. */
			arch_processor_halt();
		}
	}
}

void thread_schedule_resume(void)
{
	assert(current_thread != NULL);
	printk("Current thread: %p (%p)\n", current_thread, current_thread->processor);
	thread_schedule_resume_proc(current_thread->processor);
}

