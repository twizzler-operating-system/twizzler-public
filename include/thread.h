#pragma once
#include <thread-bits.h>
#include <ref.h>
#include <lib/inthash.h>
#include <arch/thread.h>
#include <workqueue.h>
#include <memory.h>
#include <lib/list.h>

struct processor;

enum thread_state {
	THREADSTATE_RUNNING,
	THREADSTATE_BLOCKED,
};

#define MAX_SC 32

struct kso_throbj {
	struct object *obj;
};

struct thread {
	struct ref ref;
	struct arch_thread arch;
	unsigned long id;
	_Atomic int flags;
	enum thread_state state;
	
	struct processor *processor;
	struct vm_context *ctx;

	struct spinlock sc_lock;
	struct secctx *active_sc;
	struct secctx *attached_scs[MAX_SC];

	struct kso_throbj *throbj;

	struct list rq_entry;
};

void arch_thread_start(struct thread *thread, void *jump, void *arg);
void arch_thread_initialize(struct thread *idle);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void *jump, void *arg);
_Noreturn void thread_exit(void);
void arch_thread_init(struct thread *thread, void *entry, void *arg, void *stack);

void thread_initialize_processor(struct processor *proc);

void thread_schedule_resume(void);
void thread_schedule_resume_proc(struct processor *proc);
void arch_thread_resume(struct thread *thread);

