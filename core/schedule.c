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
		/* don't enqueue the idle thread! it only runs when necessary! */
		if(current_thread != &proc->idle_thread)
			linkedlist_insert(&proc->runqueue, &current_thread->entry, current_thread);
	}
	if(proc->runqueue.count == 0)
		return &proc->idle_thread;
	return linkedlist_remove_tail(&proc->runqueue);
}

static inline void __do_schedule(void)
{
	bool s = arch_interrupt_set(false);
	unsigned long preempt_status = atomic_exchange(&current_thread->processor->preempt_disable, 0);
	struct thread *next = choose_thread(current_thread->processor);
	if(next != current_thread) {
		arch_thread_switchto(current_thread, next);
	}
	struct workqueue *wq = &current_thread->processor->wq;
	current_thread->processor->preempt_disable = preempt_status;
	arch_interrupt_set(s);
	if(workqueue_pending(wq)) {
		workqueue_dowork(wq);
	}
}

void schedule(void)
{
	__do_schedule();
}

void preempt(void)
{
	/* TODO: this may not be thread-safe when moving threads between processors */
	if(current_thread->processor->preempt_disable == 0) {
		__do_schedule();
	}
}

