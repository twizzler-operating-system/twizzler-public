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

void schedule(void)
{
	
}

