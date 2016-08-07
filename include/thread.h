#pragma once
#include <thread-bits.h>
#include <ref.h>
#include <lib/hash.h>
#include <arch/thread.h>
#include <workqueue.h>
struct processor;

enum thread_state {
	THREADSTATE_RUNNING,
	THREADSTATE_BLOCKED,
	THREADSTATE_DEAD,
};

struct thread {
	void *kernel_stack;
	void *stack_pointer;
	struct ref ref;
	unsigned long id;
	_Atomic int flags;
	enum thread_state state;
	
	struct processor *processor;

	struct hashelem elem;
	struct linkedentry entry;
	struct task del_task;
};

void arch_thread_switchto(struct thread *old, struct thread *new);
void arch_thread_start(struct thread *thread, void *jump, void *arg);
void arch_thread_initialize(struct thread *idle);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void *jump, void *arg);
_Noreturn void thread_exit(void);

#define current_thread arch_thread_get_current()

void thread_initialize_processor(struct processor *proc);

void schedule(void);
void preempt(void);

