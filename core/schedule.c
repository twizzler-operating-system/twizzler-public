#include <processor.h>
#include <debug.h>

void thread_schedule_resume_proc(struct processor *proc)
{
	if(current_thread)
		linkedlist_insert(&proc->runqueue, &current_thread->entry, current_thread);
	struct thread *next = linkedlist_remove_tail(&proc->runqueue);
	if(next)
		arch_thread_resume(next);
	printk("processor %ld halting\n", proc->id);
	for(;;);
}

void thread_schedule_resume(void)
{
	assert(current_thread != NULL);
	thread_schedule_resume_proc(current_thread->processor);
}

