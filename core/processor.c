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

void *bsp_percpu_region = NULL;

void processor_early_init(void)
{
	for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		arch_processor_early_init(&processors[i]);
		workqueue_create(&processors[i].wq);
	}
}

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
		proc->flags |= PROCESSOR_UP;
	} else {
		size_t percpu_length = (size_t)&kernel_data_percpu_length;
		proc->percpu = (void *)mm_memory_alloc(percpu_length, PM_TYPE_DRAM, true);
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
	// printk("Initializing secondary processors...\n");
	printk("[cpu] starting secondary processors\n");
	processor_count++; /* BSP */
	for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		struct processor *proc = &processors[i];
		if(!(proc->flags & PROCESSOR_BSP) && !(proc->flags & PROCESSOR_UP)
		   && (proc->flags & PROCESSOR_REGISTERED)) {
			if(arch_processor_boot(proc)) {
				processor_count++;
			} else {
				printk("[cpu] failed to start CPU %d\n", proc->id);
			}
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
	proc->flags |= PROCESSOR_UP;
	arch_processor_init(proc);
	kernel_main(proc);
}

#include <lib/iter.h>
void processor_print_stats(struct processor *proc)
{
	printk("processor %d\n", proc->id);
	printk("  thr_switch : %-ld\n", proc->stats.thr_switch);
	printk("  ext_intr   : %-ld\n", proc->stats.ext_intr);
	printk("  int_intr   : %-ld\n", proc->stats.int_intr);
	printk("  running    : %-ld\n", proc->stats.running);
	printk("  sctx_switch: %-ld\n", proc->stats.sctx_switch);
	printk("  shootdowns : %-ld\n", proc->stats.shootdowns);
	printk("  syscalls   : %-ld\n", proc->stats.syscalls);
	spinlock_acquire_save(&proc->sched_lock);
	printk("  THREADS\n");
	foreach(e, list, &proc->runqueue) {
		struct thread *t = list_entry(e, struct thread, rq_entry);
		printk("    %ld: %d\n", t->id, t->state);
	}
	spinlock_release_restore(&proc->sched_lock);
}

void processor_print_all_stats(void)
{
	for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		if((processors[i].flags & PROCESSOR_UP)) {
			processor_print_stats(&processors[i]);
		}
	}
}

static void __do_processor_attach_thread(struct processor *proc, struct thread *thread)
{
	spinlock_acquire_save(&proc->sched_lock);
	thread->processor = proc;
	list_insert(&proc->runqueue, &thread->rq_entry);
	spinlock_release_restore(&proc->sched_lock);
	proc->load++;
	proc->stats.running++;
	if(proc != current_processor) {
		arch_processor_scheduler_wakeup(proc);
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
	// printk("processor load: %ld %d\n", proc->load, list_empty(&proc->runqueue));
	__do_processor_attach_thread(proc, thread);
}

void processor_percpu_regions_init(void)
{
	size_t percpu_length = (size_t)&kernel_data_percpu_length;
	printk("[cpu] loading percpu data from %p, length %ld bytes\n",
	  &kernel_data_percpu_load,
	  percpu_length);
	if(percpu_length > mm_page_size(0))
		panic("TODO: large percpu");
	bsp_percpu_region = (void *)mm_virtual_early_alloc();
	memcpy(bsp_percpu_region, &kernel_data_percpu_load, percpu_length);
	// printk(":: %p\n", current_processor);
	// current_processor->percpu = bsp_percpu_region;
}

#include <device.h>
#include <object.h>
#include <twz/driver/system.h>
static void __init_processor_objects(void *_a __unused)
{
	struct object *so = get_system_object();
	size_t count = 0;
	struct bus_repr *brepr = bus_get_repr(so);
	for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		if((processors[i].flags & PROCESSOR_UP)) {
			struct object *d = device_register(DEVICE_BT_SYSTEM, i);
			char name[128];
			snprintf(name, 128, "CPU %d", i);
			kso_setname(d, name);

			kso_attach(so, d, brepr->max_children);
			count++;
			processors[i].obj = d; /* krc: move */
		}
	}
	device_release_headers(so);
	struct system_header *hdr = bus_get_busspecific(so);
	hdr->nrcpus = count;
	device_release_headers(so);
}
POST_INIT(__init_processor_objects, NULL);

void processor_update_stats(void)
{
	for(int i = 0; i < PROCESSOR_MAX_CPUS; i++) {
		if((processors[i].flags & PROCESSOR_UP)) {
			struct object *d = processors[i].obj;
			if(d) {
				struct processor_header *ph = device_get_devspecific(d);
				ph->stats = processors[i].stats;
				device_release_headers(d);
			}
		}
	}
}
