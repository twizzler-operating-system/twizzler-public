#include <processor.h>
#include <debug.h>
#include <thread.h>

__noinstrument
void thread_schedule_resume_proc(struct processor *proc)
{
	while(true) {
		spinlock_acquire(&proc->sched_lock);
		if(current_thread && current_thread->state == THREADSTATE_RUNNING) {
			list_insert(&proc->runqueue, &current_thread->rq_entry);
		}
		struct list *ent = list_dequeue(&proc->runqueue);
		if(ent) {
			spinlock_release(&proc->sched_lock, 0);
			arch_thread_resume(list_entry(ent, struct thread, rq_entry));
		} else {
			spinlock_release(&proc->sched_lock, 1);
			/* we're halting here, but the arch_processor_halt function will return
			 * after an interrupt is fired. Since we're in kernel-space, any interrupt
			 * we get will not invoke the scheduler. */
			arch_processor_halt();
		}
	}
}

__noinstrument
void thread_schedule_resume(void)
{
	assert(current_thread != NULL);
	thread_schedule_resume_proc(current_thread->processor);
}

void thread_sleep(struct thread *t, int flags, int64_t timeout)
{
	/* TODO (major): spec */
	t->state = THREADSTATE_BLOCKED;
}

void thread_wake(struct thread *t)
{
	t->state = THREADSTATE_RUNNING;
	if(t != current_thread) {
		spinlock_acquire_save(&t->processor->sched_lock);
		list_insert(&t->processor->runqueue, &t->rq_entry);
		spinlock_release_restore(&t->processor->sched_lock);
	}
}

