#include <processor.h>
#include <system.h>
#include <slab.h>
#include <memory.h>
#include <init.h>
#include <guard.h>

void kernel_main(struct processor *);

static struct processor processors[PROCESSOR_MAX_CPUS];

static struct processor *proc_bsp = NULL;
extern int initial_boot_stack;
void processor_register(bool bsp, unsigned long id)
{
	if(id >= PROCESSOR_MAX_CPUS) {
		printk("[kernel]: not registering cpu %ld: increase MAX_CPUS\n", id);
		return;
	}
	struct processor *proc = &processors[id];
	proc->id = id;
	if(bsp) {
		assert(proc_bsp == NULL);
		proc->flags = PROCESSOR_BSP;
		proc_bsp = proc;
	}
	proc->flags |= PROCESSOR_REGISTERED;
	list_init(&proc->runqueue);
	proc->sched_lock = SPINLOCK_INIT;
}

__orderedinitializer(PROCESSOR_INITIALIZER_ORDER) static void processor_init(void)
{
	for(unsigned int i=0;i<PROCESSOR_MAX_CPUS;i++) {
		processors[i].id = 0;
		processors[i].flags = 0;
	}
	arch_processor_enumerate();
}

static void processor_init_secondaries(void *arg __unused)
{
	printk("Initializing secondary processors...\n");
	for(int i=0;i<PROCESSOR_MAX_CPUS;i++) {
		struct processor *proc = &processors[i];
		if(!(proc->flags & PROCESSOR_BSP) && !(proc->flags & PROCESSOR_UP)
				&& (proc->flags & PROCESSOR_REGISTERED)) {
			arch_processor_boot(proc);
		}
	}
}
POST_INIT(processor_init_secondaries, NULL);

void processor_perproc_init(struct processor *proc)
{
	assert(proc_bsp != NULL);
	if(proc == NULL) {
		/* bootstrap processor */
		proc = proc_bsp;
	}
	arch_processor_init(proc);
	kernel_main(proc);
}

void processor_secondary_entry(struct processor *proc)
{
	proc->flags |= PROCESSOR_UP;
	processor_perproc_init(proc);
}

void processor_attach_thread(struct processor *proc, struct thread *thread)
{
	bool fl = spinlock_acquire(&proc->sched_lock);
	thread->processor = proc;
	list_insert(&proc->runqueue, &thread->rq_entry);
	spinlock_release(&proc->sched_lock, fl);
}

