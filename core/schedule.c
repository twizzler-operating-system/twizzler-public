#include <processor.h>
#include <thread.h>
#include <spinlock.h>
#include <guard.h>

static inline struct thread *choose_thread(struct processor *proc)
{
	spinlock_guard(&proc->sched_lock);
	if(current_thread->state == THREADSTATE_RUNNING) {
		if(proc->runqueue.count == 0)
			return current_thread;
		linkedlist_insert(&proc->runqueue, &current_thread->entry, current_thread);
	}
	if(proc->runqueue.count == 0)
		return &proc->idle_thread;
	return linkedlist_remove_tail(&proc->runqueue);
}

static inline void __do_schedule(void)
{
	struct thread *next = choose_thread(current_thread->processor);
	if(next != current_thread) {
		arch_thread_switchto(current_thread, next);
	}
}

void schedule(void)
{
	interrupt_set_scope(false);
	__do_schedule();
}

void preempt(void)
{
	interrupt_set_scope(false);
	if(current_thread->processor->preempt_disable == 0)
		__do_schedule();
}

