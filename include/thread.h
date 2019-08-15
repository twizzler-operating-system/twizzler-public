#pragma once
#include <arch/thread.h>
#include <krc.h>
#include <lib/inthash.h>
#include <lib/list.h>
#include <memory.h>
#include <thread-bits.h>
#include <workqueue.h>

#include <twz/_fault.h>
#include <twz/_thrd.h>

struct processor;

enum thread_state {
	THREADSTATE_RUNNING,
	THREADSTATE_BLOCKED,
	THREADSTATE_EXITED,
	THREADSTATE_INITING,
};

#define MAX_SC TWZ_THRD_MAX_SCS

struct thread {
	struct arch_thread arch;
	struct spinlock lock;
	unsigned long id;
	enum thread_state state;
	int64_t timeslice;
	int priority;
	struct krc refs;
	objid_t thrid;

	struct processor *processor;
	struct vm_context *ctx;

	struct spinlock sc_lock;
	struct sctx *active_sc;
	struct sctx *attached_scs[MAX_SC];
	uint32_t attached_scs_attrs[MAX_SC];
	struct sctx *attached_scs_backup[MAX_SC];
	uint32_t attached_scs_attrs_backup[MAX_SC];

	struct kso_throbj *throbj;

	struct list rq_entry, bl_entry;
};

struct arch_syscall_become_args;
void arch_thread_become(struct arch_syscall_become_args *ba);
void thread_sleep(struct thread *t, int flags, int64_t);
void thread_wake(struct thread *t);
void thread_exit(void);
void thread_raise_fault(struct thread *t, int fault, void *info, size_t);
struct timespec;
long thread_sync_single(int operation, long *addr, long arg, struct timespec *spec);
void arch_thread_raise_call(struct thread *t, void *addr, long a0, void *, size_t);

struct thread *thread_lookup(unsigned long id);
struct thread *thread_create(void);
void arch_thread_init(struct thread *thread,
  void *entry,
  void *arg,
  void *stack,
  size_t stacksz,
  void *tls,
  size_t);

void thread_initialize_processor(struct processor *proc);

void thread_schedule_resume(void);
void thread_schedule_resume_proc(struct processor *proc);
void arch_thread_resume(struct thread *thread);
uintptr_t arch_thread_instruction_pointer(void);
