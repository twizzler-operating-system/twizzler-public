#include <processor.h>
#include <debug.h>
#include <thread.h>
#include <clksrc.h>

static long last = -1;
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
			bool empty = list_empty(&proc->runqueue);
			spinlock_release(&proc->sched_lock, 0);
			struct thread *next = list_entry(ent, struct thread, rq_entry);
			next->timeslice = 100000 + next->priority * 100000;
			if(!empty) {
				clksrc_set_interrupt_countdown(next->timeslice, false);
				if(next->priority > 1) next->priority--;
			}
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
	t->priority *= 20;
	if(t->priority > 1000) {
		t->priority = 1000;
	}
	t->state = THREADSTATE_BLOCKED;
}

void thread_wake(struct thread *t)
{
	assert(t->state == THREADSTATE_BLOCKED);
	t->state = THREADSTATE_RUNNING;
	if(t != current_thread) {
		spinlock_acquire_save(&t->processor->sched_lock);
		if(list_empty(&t->processor->runqueue)) {
			if(t->processor != current_processor) {
				processor_send_ipi(t->processor->id, PROCESSOR_IPI_RESUME,
						NULL, PROCESSOR_IPI_NOWAIT);
			}
		}
		list_insert(&t->processor->runqueue, &t->rq_entry);
		spinlock_release_restore(&t->processor->sched_lock);
	}
}

void thread_exit(void)
{
	current_thread->state = THREADSTATE_EXITED;
	assert(current_processor->load > 0);
	current_processor->load--;
	/* TODO (major): cleanup thread resources */
}

struct thread *thread_create(void)
{
	struct thread *t = slabcache_alloc(&_sc_thread);
	krc_init(&t->refs);
	t->priority = 10;
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
	if(fi.addr) {
		arch_thread_raise_call(t, fi.addr, fault, info, infolen);
	} else {
		printk("unhandled fault: %ld: %d\n", t->id, fault);
		/* TODO (major): raise unhandled fault exception? */
		thread_exit();
	}
}

