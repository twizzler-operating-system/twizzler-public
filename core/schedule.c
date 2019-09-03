#include <clksrc.h>
#include <debug.h>
#include <processor.h>
#include <thread.h>

#define TIMESLICE 1000000

__noinstrument void thread_schedule_resume_proc(struct processor *proc)
{
	while(true) {
		/* TODO (major): allow current thread to run again */
		spinlock_acquire(&proc->sched_lock);

		if(current_thread && current_thread->timeslice
		   && current_thread->state == THREADSTATE_RUNNING) {
			if(proc->id == 3) {
				printk(
				  "resuming current: %ld (%ld)\n", current_thread->id, current_thread->timeslice);
			}
			clksrc_set_interrupt_countdown(current_thread->timeslice, false);
			spinlock_release(&proc->sched_lock, 0);

			arch_thread_resume(current_thread);
		}

		//	if(current_thread && current_thread->state == THREADSTATE_RUNNING) {
		//		list_insert(&proc->runqueue, &current_thread->rq_entry);
		//	} else if(current_thread && current_thread->state == THREADSTATE_BLOCKING) {
		//		current_thread->state = THREADSTATE_BLOCKED;
		//	}
		struct list *ent = list_dequeue(&proc->runqueue);
		if(ent) {
			bool empty = list_empty(&proc->runqueue);
			struct thread *next = list_entry(ent, struct thread, rq_entry);
			list_insert(&proc->runqueue, &next->rq_entry);
			spinlock_release(&proc->sched_lock, 0);

			next->timeslice = TIMESLICE; // + next->priority * 100000;
			if(proc->id == 3) {
				printk("%ld: %ld (%d) %ld\n",
				  next->id,
				  next->timeslice,
				  empty,
				  current_thread ? current_thread->arch.exception.int_no : 0);
			}
			if(!empty) {
				clksrc_set_interrupt_countdown(next->timeslice, false);
				if(next->priority > 1)
					next->priority--;
			}
			assertmsg(next->state == THREADSTATE_RUNNING,
			  "%ld threadstate is %d (%d)",
			  next->id,
			  next->state,
			  empty);
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

void __schedule_timer_handler(int v, struct interrupt_handler *hdl)
{
	(void)v;
	(void)hdl;
	if(current_thread) {
		if(current_thread->processor->id == 3)
			printk("%ld TIMER: %ld\n", current_thread->id, clksrc_get_interrupt_countdown());
		current_thread->timeslice = clksrc_get_interrupt_countdown();
	}
}

struct interrupt_handler _timer_handler = {
	.fn = __schedule_timer_handler,
};

__initializer static void __init_int_timer(void)
{
	/* TODO: arch-dep */
	interrupt_register_handler(32, &_timer_handler);
}

#include <slab.h>

static _Atomic unsigned long _internal_tid_counter = 0;

static void _thread_ctor(void *_u __unused, void *ptr)
{
	struct thread *thr = ptr;
	thr->id = ++_internal_tid_counter;
	thr->sc_lock = SPINLOCK_INIT;
	thr->lock = SPINLOCK_INIT;
	thr->state = THREADSTATE_INITING;
}

static DECLARE_SLABCACHE(_sc_thread, sizeof(struct thread), _thread_ctor, NULL, NULL);

__noinstrument void thread_schedule_resume(void)
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
	/* TODO: threadsafe? */
	bool in = arch_interrupt_set(false);
	if(t->state != THREADSTATE_BLOCKED) {
		t->state = THREADSTATE_BLOCKED;
		list_remove(&t->rq_entry);
	}
	arch_interrupt_set(in);
}

void thread_wake(struct thread *t)
{
	assert(t->state == THREADSTATE_BLOCKED);

	spinlock_acquire_save(&t->processor->sched_lock);
	int old = atomic_exchange(&t->state, THREADSTATE_RUNNING);
	if(old == THREADSTATE_BLOCKED) {
		list_insert(&t->processor->runqueue, &t->rq_entry);
		if(t->processor != current_processor) {
			processor_send_ipi(t->processor->id, PROCESSOR_IPI_RESUME, NULL, PROCESSOR_IPI_NOWAIT);
		}
	}
	spinlock_release_restore(&t->processor->sched_lock);
}
static DECLARE_LIST(allthreads);

void thread_exit(void)
{
	list_remove(&current_thread->rq_entry);
	current_thread->state = THREADSTATE_EXITED;
	assert(current_processor->load > 0);
	current_processor->load--;
	list_remove(&current_thread->all_entry);
	/* TODO (major): cleanup thread resources */
}
#include <lib/iter.h>
void thread_print_all_threads(void)
{
	foreach(e, list, &allthreads) {
		struct thread *t = list_entry(e, struct thread, all_entry);

		spinlock_acquire_save(&t->lock);
		printk("thread %ld\n", t->id);
		printk("  CPU: %d\n", t->processor ? (int)t->processor->id : -1);
		printk("  state: %d\n", t->state);
		spinlock_release_restore(&t->lock);
	}
}

struct thread *thread_create(void)
{
	struct thread *t = slabcache_alloc(&_sc_thread);
	krc_init(&t->refs);
	t->priority = 10;
	list_insert(&allthreads, &t->all_entry);
	return t;
}

#include <debug.h>
#include <object.h>
#include <twz/_thrd.h>

static void __print_fault_info(struct thread *t, int fault, void *info)
{
	printk("unhandled fault: %ld: %d\n", t->id, fault);
	debug_print_backtrace();
	switch(fault) {
		struct fault_object_info *foi;
		struct fault_null_info *fni;
		struct fault_exception_info *fei;
		struct fault_sctx_info *fsi;
		case FAULT_OBJECT:
			foi = info;
			printk("foi->objid " IDFMT "\n", IDPR(foi->objid));
			printk("foi->ip    %lx\n", foi->ip);
			printk("foi->addr  %lx\n", foi->addr);
			printk("foi->flags %lx\n", foi->flags);
			break;
		case FAULT_NULL:
			fni = info;
			printk("fni->ip    %lx\n", fni->ip);
			printk("fni->addr  %lx\n", fni->addr);
			break;
		case FAULT_EXCEPTION:
			fei = info;
			printk("fei->ip    %lx\n", fei->ip);
			printk("fei->code  %lx\n", fei->code);
			printk("fei->arg0  %lx\n", fei->arg0);
			break;
		case FAULT_SCTX:
			fsi = info;
			printk("fsi->target " IDFMT "\n", IDPR(fsi->target));
			printk("fsi->ip     %lx\n", fsi->ip);
			printk("fsi->addr   %lx\n", fsi->addr);
			printk("fsi->pneed  %x\n", fsi->pneed);
			break;
	}
}

void thread_raise_fault(struct thread *t, int fault, void *info, size_t infolen)
{
	struct object *to = kso_get_obj(t->throbj, thr);
	if(!to) {
		panic("No repr");
	}
	struct faultinfo fi;
	obj_read_data(
	  to, offsetof(struct twzthread_repr, faults) + sizeof(fi) * fault, sizeof(fi), &fi);
	if(fi.view) {
		panic("NI - different view :: %d", fault);
	}
	if(fi.addr) {
		arch_thread_raise_call(t, fi.addr, fault, info, infolen);
	} else {
		struct faultinfo fi_f;
		obj_read_data(to,
		  offsetof(struct twzthread_repr, faults) + sizeof(fi_f) * FAULT_FAULT,
		  sizeof(fi_f),
		  &fi_f);
		if(fi_f.addr) {
			/* thread can catch an unhandled fault */
			struct fault_fault_info ffi = {
				.fault_nr = fault,
				.info = 0,
				.resv = 0,
				.len = infolen,
			};
			size_t nl = infolen + sizeof(ffi);
			nl = ((nl - 1) & ~0xf) + 0x10; /* infolen must be 16 aligned */
			char tmp[nl];
			memcpy(tmp, &ffi, sizeof(ffi));
			memcpy(tmp + sizeof(ffi), info, infolen);
			arch_thread_raise_call(t, fi_f.addr, FAULT_FAULT, tmp, nl);
		} else {
			__print_fault_info(t, fault, info);
			thread_exit();
		}
	}
}
