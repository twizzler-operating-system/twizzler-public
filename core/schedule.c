#include <processor.h>
#include <debug.h>
#include <thread.h>

void thread_schedule_resume_proc(struct processor *proc)
{
	while(true) {
		spinlock_acquire_save(&proc->sched_lock);
		if(current_thread && current_thread->state == THREADSTATE_RUNNING) {
			list_insert(&proc->runqueue, &current_thread->rq_entry);
		}
		struct list *ent = list_dequeue(&proc->runqueue);
		spinlock_release_restore(&proc->sched_lock);
		if(ent) {
			arch_thread_resume(list_entry(ent, struct thread, rq_entry));
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
	thread_schedule_resume_proc(current_thread->processor);
}

