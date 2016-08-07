#pragma once

#include <arch/processor.h>
#include <thread.h>
#include <lib/linkedlist.h>
#include <lib/hash.h>
#include <interrupt.h>
#include <workqueue.h>

#define PROCESSOR_UP     1
#define PROCESSOR_ACTIVE 2
#define PROCESSOR_BSP    4

struct processor {
	struct linkedlist runqueue;
	struct spinlock sched_lock;
	_Atomic int flags;
	unsigned long id;
	_Atomic unsigned int preempt_disable;

	void *initial_stack;
	struct thread idle_thread;

	struct workqueue wq;

	struct hashelem elem;
};

void processor_perproc_init(struct processor *proc);

void processor_register(bool bsp, unsigned long id);
void arch_processor_enumerate(void);
void arch_processor_boot(struct processor *proc);

static inline void processor_disable_preempt(void)
{
	if(current_thread == NULL)
		return;
	interrupt_set_scope(false);
	struct processor *proc = current_thread->processor;
	proc->preempt_disable++;
}

static inline void processor_enable_preempt(void)
{
	if(current_thread == NULL)
		return;
	interrupt_set_scope(false);
	struct processor *proc = current_thread->processor;
	assert(proc->preempt_disable > 0);
	proc->preempt_disable--;
}

void processor_attach_thread(struct processor *proc, struct thread *thread);

