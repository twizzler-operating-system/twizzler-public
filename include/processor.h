#pragma once

#include <arch/processor.h>
#include <thread.h>
#include <lib/list.h>
#include <interrupt.h>
#include <workqueue.h>

#define PROCESSOR_UP     1
#define PROCESSOR_ACTIVE 2
#define PROCESSOR_BSP    4
#define PROCESSOR_REGISTERED 8

#define PROCESSOR_INITIALIZER_ORDER 0

#ifdef CONFIG_MAX_CPUS
#define PROCESSOR_MAX_CPUS CONFIG_MAX_CPUS
#else
#define PROCESSOR_MAX_CPUS 64
#endif

struct processor {
	struct arch_processor arch;
	struct list runqueue;
	struct spinlock sched_lock;
	_Atomic int flags;
	unsigned long id;
};

void processor_perproc_init(struct processor *proc);

void processor_register(bool bsp, unsigned long id);
void arch_processor_enumerate(void);
void arch_processor_boot(struct processor *proc);
void processor_secondary_entry(struct processor *proc);
void processor_attach_thread(struct processor *proc, struct thread *thread);
void arch_processor_init(struct processor *proc);

