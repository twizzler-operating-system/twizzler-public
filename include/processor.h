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

#define PROCESSOR_INITIALIZER_ORDER 0

struct processor {
	struct arch_processor arch;
	struct linkedlist runqueue;
	struct spinlock sched_lock;
	_Atomic int flags;
	unsigned long id;

	struct hashelem elem;
};

void processor_perproc_init(struct processor *proc);

void processor_register(bool bsp, unsigned long id);
void arch_processor_enumerate(void);
void arch_processor_boot(struct processor *proc);

