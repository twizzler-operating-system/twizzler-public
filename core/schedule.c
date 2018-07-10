#include <processor.h>
#include <debug.h>
#include <thread.h>
#include <clksrc.h>

__noinstrument
void thread_schedule_resume_proc(struct processor *proc)
{
	while(true) {
		/* TODO (major): allow current thread to run again */
		spinlock_acquire(&proc->sched_lock);
		if(current_thread && current_thread->state == THREADSTATE_RUNNING) {
			list_insert(&proc->runqueue, &current_thread->rq_entry);
		}
		struct list *ent = list_dequeue(&proc->runqueue);
		if(ent) {
			spinlock_release(&proc->sched_lock, 0);
			struct thread *next = list_entry(ent, struct thread, rq_entry);
			next->timeslice = 100000; /* 100 us. TODO (major): make this dynamic */
			clksrc_set_interrupt_countdown(next->timeslice, false);
			arch_thread_resume(next);
		} else {
			spinlock_release(&proc->sched_lock, 1);
			/* we're halting here, but the arch_processor_halt function will return
			 * after an interrupt is fired. Since we're in kernel-space, any interrupt
			 * we get will not invoke the scheduler. */
			arch_processor_halt();
		}
	}
}

#include <slab.h>

static _Atomic unsigned long _internal_tid_counter = 0;

static void _thread_ctor(void *_u __unused, void *ptr)
{
	struct thread *thr = ptr;
	thr->id = ++_internal_tid_counter;
	thr->sc_lock = SPINLOCK_INIT;
	thr->state = THREADSTATE_INITING;
}

static DECLARE_SLABCACHE(_sc_thread, sizeof(struct thread), _thread_ctor, NULL, NULL);

__noinstrument
void thread_schedule_resume(void)
{
	assert(current_thread != NULL);
	thread_schedule_resume_proc(current_processor);
}

void thread_sleep(struct thread *t, int flags, int64_t timeout)
{
	/* TODO (major): timeout */
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

void thread_exit(void)
{
	current_thread->state = THREADSTATE_EXITED;
	/* TODO (major): cleanup thread resources */
}

struct thread *thread_create(void)
{
	struct thread *t = slabcache_alloc(&_sc_thread);
	krc_init(&t->refs);
	return t;
}

#include <object.h>
void thread_raise_fault(struct thread *t, int fault, void *info, size_t infolen)
{
	struct object *to = kso_get_obj(t->throbj, thr);
	if(!to) {
		panic("No repr");
	}
	struct faultinfo fi;
	obj_read_data(to, sizeof(fi) * fault, sizeof(fi), &fi); 
	if(fi.view) {
		panic("NI - different view");
	}
	printk(":: FAULT: %p\n", fi.addr);
	if(fi.addr) {
		arch_thread_raise_call(t, fi.addr, fault, info, infolen);
	}
}

