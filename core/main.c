#include <debug.h>
#include <memory.h>
#include <init.h>
#include <arena.h>
#include <processor.h>
#include <time.h>
#include <thread.h>

static struct arena post_init_call_arena;
static struct init_call *post_init_call_head = NULL;

void post_init_call_register(void (*fn)(void *), void *data)
{
	if(post_init_call_head == NULL) {
		arena_create(&post_init_call_arena);
	}

	struct init_call *ic = arena_allocate(&post_init_call_arena, sizeof(struct init_call));
	ic->fn = fn;
	ic->data = data;
	ic->next = post_init_call_head;
	post_init_call_head = ic;
}

static void post_init_calls_execute(void)
{
	for(struct init_call *call = post_init_call_head; call != NULL; call = call->next) {
		call->fn(call->data);
	}
	arena_destroy(&post_init_call_arena);
	post_init_call_head = NULL;
}

/* functions called from here expect virtual memory to be set up. However, functions
 * called from here cannot rely on global contructors having run, as those are allowed
 * to use memory management routines, so they are run after this.
 */
void kernel_early_init(void)
{
	mm_init();
}

/* at this point, memory management, interrupt routines, global constructors, and shared
 * kernel state between nodes have been initialized. Now initialize all application processors
 * and per-node threading.
 */

void kernel_init(void)
{
	post_init_calls_execute();
	processor_perproc_init(NULL);
}

struct thread init_thread;
char us1[0x1000];

void kernel_main(struct processor *proc)
{
	printk("processor %ld reached resume state %p\n", proc->id, proc);
	
	if(proc->flags & PROCESSOR_BSP) {
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
	thread_schedule_resume_proc(proc);
}

