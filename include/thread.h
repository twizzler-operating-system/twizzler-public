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

#define THREAD_SCHEDULE 1

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

/* blocking */

enum block_result {
	BLOCKRES_BLOCKED,
	BLOCKRES_UNBLOCKED,
	BLOCKRES_TIMEOUT,
	BLOCKRES_INTERRUPT,
};

#define BLOCKPOINT_UNINTERRUPT 1

struct blocklist {
	struct spinlock lock;
	struct linkedlist list;
};

struct blockpoint {
	struct blocklist *blocklist;
	struct thread *thread;
	int flags;
	enum block_result result;
	struct linkedentry entry;
};

enum block_result blockpoint_cleanup(struct blockpoint *bp);
void blocklist_wake(struct blocklist *bl, int n);
void blocklist_attach(struct blocklist *bl, struct blockpoint *bp);
void blockpoint_create(struct blockpoint *bp, int);
void blocklist_create(struct blocklist *bl);

