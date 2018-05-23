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
	unsigned int id;
	void *percpu;
};

void processor_perproc_init(struct processor *proc);
void processor_percpu_regions_init(void);

void processor_register(bool bsp, unsigned int id);
void arch_processor_enumerate(void);
void arch_processor_boot(struct processor *proc);
void processor_secondary_entry(struct processor *proc);
uint64_t arch_processor_get_nanoseconds(void);
void processor_attach_thread(struct processor *proc, struct thread *thread);
void arch_processor_init(struct processor *proc);
void processor_init_secondaries(void);
void processor_barrier(_Atomic unsigned int *here);
struct processor *processor_get_current(void);
unsigned int arch_processor_current_id(void);

#define current_processor \
	processor_get_current()

#define DECLARE_PER_CPU(type, name) \
	__attribute__((section(".data.percpu"))) type __per_cpu_var_##name

#define PTR_ADVANCE(ptr, off) \
	({ uintptr_t p = (uintptr_t)(ptr); (typeof(ptr)) (p + (off)); }) 

#define __per_cpu_var_lea(name, proc) \
	PTR_ADVANCE(& __per_cpu_var_##name, (uintptr_t)((proc)->percpu))

#define per_cpu_get(name) \
	__per_cpu_var_lea(name, current_processor)


