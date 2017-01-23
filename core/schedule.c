#include <processor.h>

/* TODO: would rather not need this function */
struct processor *processor_get_current(void);
void thread_schedule_resume(void)
{
	struct processor *proc = processor_get_current();
	struct thread *next = linkedlist_remove_tail(&proc->runqueue);
	if(current_thread)
		linkedlist_insert(&proc->runqueue, &current_thread->entry, current_thread);
	arch_thread_resume(next);
}

