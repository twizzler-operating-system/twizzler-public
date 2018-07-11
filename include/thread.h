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
	FAULT_EXCEPTION,
	NUM_FAULTS,
};

struct faultinfo {
	objid_t view;
	void *addr;
	uint64_t flags;
} __packed;

#define FAULT_OBJECT_READ  1
#define FAULT_OBJECT_WRITE 2
#define FAULT_OBJECT_EXEC  4
#define FAULT_OBJECT_NOMAP 8
#define FAULT_OBJECT_EXIST 16

struct fault_object_info {
	objid_t objid;
	uint64_t ip;
	uint64_t addr;
	uint64_t flags;
	uint64_t pad;
} __packed;

struct fault_null_info {
	uint64_t ip;
	uint64_t addr;
} __packed;

struct fault_exception_info {
	uint64_t ip;
	uint64_t code;
	uint64_t arg0;
} __packed;

struct thread {
	struct arch_thread arch;
	unsigned long id;
	enum thread_state state;
	int64_t timeslice;
	struct krc refs;
	
	struct processor *processor;
	struct vm_context *ctx;

	struct spinlock sc_lock;
	struct secctx *active_sc;
	struct secctx *attached_scs[MAX_SC];

	struct kso_throbj *throbj;

	struct list rq_entry;
};

struct arch_syscall_become_args;
void arch_thread_become(struct arch_syscall_become_args *ba);
void thread_sleep(struct thread *t, int flags, int64_t);
void thread_wake(struct thread *t);
void thread_exit(void);
void thread_raise_fault(struct thread *t, int fault, void *info, size_t);
void arch_thread_raise_call(struct thread *t, void *addr, long a0, void *, size_t);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void);
void arch_thread_init(struct thread *thread, void *entry, void *arg,
		void *stack, size_t stacksz, void *tls);

void thread_initialize_processor(struct processor *proc);

void thread_schedule_resume(void);
void thread_schedule_resume_proc(struct processor *proc);
void arch_thread_resume(struct thread *thread);

