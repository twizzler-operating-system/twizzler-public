#include <processor.h>
#include <system.h>
#include <slab.h>
#include <memory.h>
#include <init.h>
#include <guard.h>

struct hash processors;

static void _proc_create(void *o)
{
	struct processor *proc = o;
	linkedlist_create(&proc->runqueue, LINKEDLIST_LOCKLESS);
	proc->sched_lock = SPINLOCK_INIT;
}

struct slab_allocator so_processor = SLAB_ALLOCATOR(sizeof(struct processor), 64, _proc_create, NULL, NULL, NULL);

static struct processor *proc_bsp = NULL;

extern int initial_boot_stack;
void processor_register(bool bsp, unsigned long id)
{
	struct processor *proc = slab_alloc(&so_processor);
	proc->id = id;
	if(bsp) {
		assert(proc_bsp == NULL);
		proc->flags = PROCESSOR_BSP;
		proc_bsp = proc;
		proc->kernel_stack = &initial_boot_stack;
	} else {
		proc->kernel_stack = (void *)mm_virtual_alloc(KERNEL_STACK_SIZE, PM_TYPE_ANY, true);
	}
	hash_insert(&processors, &proc->id, sizeof(proc->id), &proc->elem, proc);
}

__orderedinitializer(PROCESSR_INITIALIZER_ORDER) static void processor_init(void)
{
	hash_create(&processors, 64, 0);
	arch_processor_enumerate();
	/* enumerate processors on system */
}

static void processor_init_secondaries(void *arg)
{
	(void)arg;
	printk("Initializing secondary processors...\n");
	struct hashiter iter;
	__hash_lock(&processors);
	for(hash_iter_init(&iter, &processors);
			!hash_iter_done(&iter);
			hash_iter_next(&iter)) {
		struct processor *proc = hash_iter_get(&iter);
		if(!(proc->flags & PROCESSOR_BSP) && !(proc->flags & PROCESSOR_UP))
			arch_processor_boot(proc);
	}
	__hash_unlock(&processors);
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
}

void kernel_idle(void);
void processor_secondary_entry(struct processor *proc)
{
	proc->flags |= PROCESSOR_UP;
	processor_perproc_init(proc);
	kernel_idle();
}

void processor_attach_thread(struct processor *proc, struct thread *thread)
{
	spinlock_guard(&proc->sched_lock);
	thread->processor = proc;
	linkedlist_insert(&proc->runqueue, &thread->entry, thread);
}

