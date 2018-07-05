#pragma once
#include <thread-bits.h>
#include <lib/inthash.h>
#include <arch/thread.h>
#include <workqueue.h>
#include <memory.h>
#include <lib/list.h>
#include <krc.h>

struct processor;

enum thread_state {
	THREADSTATE_RUNNING,
	THREADSTATE_BLOCKED,
	THREADSTATE_EXITED,
	THREADSTATE_INITING,
};

#define MAX_SC 32

enum {
	FAULT_OBJECT,
	FAULT_NULL,
	NUM_FAULTS,
};

struct faultinfo {
	objid_t view;
	void *addr;
	uint64_t flags;
};

struct thread {
	struct arch_thread arch;
	unsigned long id;
	enum thread_state state;
	struct krc refs;
	
	struct processor *processor;
	struct vm_context *ctx;

	struct spinlock sc_lock;
	struct secctx *active_sc;
	struct secctx *attached_scs[MAX_SC];

	struct kso_throbj *throbj;

	struct list rq_entry;
	
	struct faultinfo faults[NUM_FAULTS];
};

struct arch_syscall_become_args;
void arch_thread_become(struct arch_syscall_become_args *ba);
void thread_sleep(struct thread *t, int flags, int64_t);
void thread_wake(struct thread *t);
void thread_exit(void);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void);
void arch_thread_init(struct thread *thread, void *entry, void *arg,
		void *stack, size_t stacksz, void *tls);

void thread_initialize_processor(struct processor *proc);

void thread_schedule_resume(void);
void thread_schedule_resume_proc(struct processor *proc);
void arch_thread_resume(struct thread *thread);

