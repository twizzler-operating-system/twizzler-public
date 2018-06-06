#include <debug.h>
#include <clksrc.h>
#include <memory.h>
#include <init.h>
#include <arena.h>
#include <processor.h>
#include <time.h>
#include <thread.h>

static struct arena post_init_call_arena;
static struct init_call *post_init_call_head = NULL;

void post_init_call_register(bool ac, void (*fn)(void *), void *data)
{
	if(post_init_call_head == NULL) {
		arena_create(&post_init_call_arena);
	}

	struct init_call *ic = arena_allocate(&post_init_call_arena, sizeof(struct init_call));
	ic->fn = fn;
	ic->data = data;
	ic->allcpus = ac;
	ic->next = post_init_call_head;
	post_init_call_head = ic;
}

static void post_init_calls_execute(bool secondary)
{
	for(struct init_call *call = post_init_call_head; call != NULL; call = call->next) {
		if(!secondary || call->allcpus) {
			call->fn(call->data);
		}
	}
}

/* functions called from here expect virtual memory to be set up. However, functions
 * called from here cannot rely on global contructors having run, as those are allowed
 * to use memory management routines, so they are run after this. Furthermore,
 * they cannot use per-cpu data.
 */
void kernel_early_init(void)
{
	mm_init();
	processor_percpu_regions_init();
}

/* at this point, memory management, interrupt routines, global constructors, and shared
 * kernel state between nodes have been initialized. Now initialize all application processors
 * and per-node threading.
 */

void kernel_init(void)
{
	processor_init_secondaries();
	processor_perproc_init(NULL);
}

struct thread init_thread;
char us1[0x1000];

static void bench(void)
{
	printk("Starting benchmark\n");
	arch_interrupt_set(true);
	int c = 0;
	while(true)
	{
		//uint64_t sr = rdtsc();
		//uint64_t start = clksrc_get_nanoseconds();
		//uint64_t end = clksrc_get_nanoseconds();
		//uint64_t er = rdtsc();
		//printk(":: %ld %ld\n", end - start, er - sr);
		//printk(":: %ld\n", er - sr);

#if 0
		uint64_t start = clksrc_get_nanoseconds();
		volatile int i;
		uint64_t c = 0;
		for(i=0;i<1000000000;i++) {
			volatile int x = i ^ (i-1);
		//	uint64_t x = rdtsc();
			//clksrc_get_nanoseconds();
		//	uint64_t y = rdtsc();
		//	c += (y - x);
		}
		uint64_t end = clksrc_get_nanoseconds();
		printk("Done: %ld (%ld)\n", end - start, (end - start) / i);
		//printk("RD: %ld (%ld)\n", c, c / i);
		start = clksrc_get_nanoseconds();
		for(i=0;i<1000000000;i++) {
			us1[i % 0x1000] = i&0xff;
		}
		end = clksrc_get_nanoseconds();
		printk("MEMD: %ld (%ld)\n", end - start, (end - start) / i);
#else
		while(true) {
			uint64_t t = clksrc_get_nanoseconds();
			if(((t / 1000000) % 1000) == 0)
				printk("ONE SECOND %ld\n", t);
		}
		uint64_t start = clksrc_get_nanoseconds();
		//for(long i=0;i<800000000l;i++);
		for(long i=0;i<800000000l;i++);
		uint64_t end = clksrc_get_nanoseconds();
		printk("Done: %ld\n", end - start);
		if(c++ == 10)
			panic("reset");
#endif
	}
}

static _Atomic unsigned int kernel_main_barrier = 0;
void kernel_main(struct processor *proc)
{
	post_init_calls_execute(!(proc->flags & PROCESSOR_BSP));

	printk("Waiting at kernel_main_barrier\n");
	processor_barrier(&kernel_main_barrier);


	if(proc->flags & PROCESSOR_BSP) {
		bench();
		arena_destroy(&post_init_call_arena);
		post_init_call_head = NULL;

		init_thread.id = 1;
		init_thread.ctx = vm_context_create();
		vm_context_map(init_thread.ctx, 1, 0x7ff000001000 / mm_page_size(MAX_PGLEVEL),
				VMAP_READ | VMAP_EXEC);
		vm_context_map(init_thread.ctx, 2, 0x1000 / mm_page_size(MAX_PGLEVEL),
				VMAP_READ | VMAP_EXEC);
		//arch_thread_init(&init_thread, (void *)0x7ff000001000, NULL, us1 + 0x1000);
		arch_thread_init(&init_thread, (void *)0x400078, NULL, us1 + 0x1000);
		processor_attach_thread(proc, &init_thread);
	}
	printk("processor %d reached resume state %p\n", proc->id, proc);
	thread_schedule_resume_proc(proc);
}

