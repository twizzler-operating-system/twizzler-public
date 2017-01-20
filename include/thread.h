#pragma once
#include <thread-bits.h>
#include <ref.h>
#include <lib/hash.h>
#include <arch/thread.h>
#include <workqueue.h>
#include <memory.h>

struct processor;

enum thread_state {
	THREADSTATE_RUNNING,
	THREADSTATE_BLOCKED,
};

struct thread {
	struct ref ref;
	unsigned long id;
	_Atomic int flags;
	enum thread_state state;
	
	struct processor *processor;
	struct vm_context *ctx;

	struct hashelem elem;
	struct linkedentry entry;
};

void arch_thread_start(struct thread *thread, void *jump, void *arg);
void arch_thread_initialize(struct thread *idle);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void *jump, void *arg);
_Noreturn void thread_exit(void);

void thread_initialize_processor(struct processor *proc);

void schedule(void);

