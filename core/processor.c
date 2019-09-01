#include <guard.h>
#include <init.h>
#include <memory.h>
#include <processor.h>
#include <slab.h>
#include <system.h>

void kernel_main(struct processor *);

static struct processor processors[PROCESSOR_MAX_CPUS];

__noinstrument struct processor *processor_get_current(void)
{
	return &processors[arch_processor_current_id()];
}

static struct processor *proc_bsp = NULL;
static _Atomic unsigned int processor_count = 0;
extern int initial_boot_stack;
extern int kernel_data_percpu_load;
extern int kernel_data_percpu_length;

static void *bsp_percpu_region = NULL;

void processor_register(bool bsp, unsigned int id)
{
	if(id >= PROCESSOR_MAX_CPUS) {
		printk("[kernel]: not registering cpu %d: increase MAX_CPUS\n", id);
		return;
	}

	struct processor *proc = &processors[id];
	proc->id = id;
	if(bsp) {
		assert(proc_bsp == NULL);
		proc->flags = PROCESSOR_BSP;
		proc_bsp = proc;
		proc->percpu = bsp_percpu_region;
	} else {
		size_t percpu_length = (size_t)&kernel_data_percpu_length;
		proc->percpu = (void *)mm_virtual_alloc(percpu_length, PM_TYPE_DRAM, true);
		memcpy(proc->percpu, &kernel_data_percpu_load, percpu_length);
	}
	proc->flags |= PROCESSOR_REGISTERED;
	list_init(&proc->runqueue);
	proc->sched_lock = SPINLOCK_INIT;
}

__orderedinitializer(PROCESSOR_INITIALIZER_ORDER) static void processor_init(void)
{
	for(unsigned int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		processors[i].id = 0;
		processors[i].flags = 0;
	}
	arch_processor_enumerate();
}

void processor_barrier(_Atomic unsigned int *here)
{
	unsigned int backoff = 1;
	(*here)++;
	while(*here != processor_count) {
		for(unsigned int i = 0; i < backoff; i++) {
			arch_processor_relax();
		}
		backoff = backoff < 1000 ? backoff + 1 : backoff;
	}
}

static _Atomic int __ipi_lock = 0;
static _Atomic void *__ipi_arg;
static _Atomic unsigned int __ipi_barrier;
static _Atomic int __ipi_flags;
void processor_send_ipi(int destid, int vector, void *arg, int flags)
{
	/* cannot use a normal spinlock here, because if we spin after disabling
	 * interrupts, there's a race condition */
	while(atomic_fetch_or(&__ipi_lock, 1)) {
		arch_processor_relax();
	}

	if(!(flags & PROCESSOR_IPI_NOWAIT)) {
		__ipi_arg = arg;
		__ipi_flags = flags;
		__ipi_barrier = 0;
	}
	arch_processor_send_ipi(destid, vector, flags);

	if(destid == PROCESSOR_IPI_DEST_OTHERS) {
		if(!(flags & PROCESSOR_IPI_NOWAIT)) {
			processor_barrier(&__ipi_barrier);
		}
	}
	__ipi_lock = 0;
}

void processor_shutdown(void)
{
	current_processor->flags &= ~PROCESSOR_UP;
	processor_count--;
}

void processor_ipi_finish(void)
{
	if(__ipi_flags & PROCESSOR_IPI_BARRIER) {
		processor_barrier(&__ipi_barrier);
	} else {
		__ipi_barrier++;
	}
}

void processor_init_secondaries(void)
{
	printk("Initializing secondary processors...\n");
	processor_count++; /* BSP */
	for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		struct processor *proc = &processors[i];
		if(!(proc->flags & PROCESSOR_BSP) && !(proc->flags & PROCESSOR_UP)
		   && (proc->flags & PROCESSOR_REGISTERED)) {
			/* TODO (major): check for failure */
			arch_processor_boot(proc);
			processor_count++;
		}
	}
}

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

static void __do_processor_attach_thread(struct processor *proc, struct thread *thread)
{
	spinlock_acquire_save(&proc->sched_lock);
	thread->processor = proc;
	list_insert(&proc->runqueue, &thread->rq_entry);
	spinlock_release_restore(&proc->sched_lock);
	proc->load++;
	if(proc != current_processor) {
		processor_send_ipi(proc->id, PROCESSOR_IPI_RESUME, NULL, PROCESSOR_IPI_NOWAIT);
	}
}

void processor_attach_thread(struct processor *proc, struct thread *thread)
{
	if(proc == NULL) {
		proc = current_processor;
		for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
			if((processors[i].flags & PROCESSOR_UP) && processors[i].load < proc->load) {
				proc = &processors[i];
			}
		}
	}
	__do_processor_attach_thread(proc, thread);
}

void processor_percpu_regions_init(void)
{
	size_t percpu_length = (size_t)&kernel_data_percpu_length;
	printk(
	  "loading percpu data from %p, length %ld bytes\n", &kernel_data_percpu_load, percpu_length);
	bsp_percpu_region = (void *)mm_virtual_alloc(percpu_length, PM_TYPE_DRAM, true);
	memcpy(bsp_percpu_region, &kernel_data_percpu_load, percpu_length);
}
