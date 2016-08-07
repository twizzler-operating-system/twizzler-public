#include <debug.h>
#include <memory.h>
#include <init.h>
#include <arena.h>
#include <processor.h>

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

void kernel_init_thread(void)
{
	printk("kernel init thread reached\n");
	thread_exit();
}

void kernel_idle(void)
{
	printk("reached idle state\n");
	while(true) {
		schedule();
	}
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
void kernel_main(void)
{
	post_init_calls_execute();

	processor_perproc_init(NULL);
	processor_attach_thread(current_thread->processor, thread_create(kernel_init_thread, NULL));
	kernel_idle();
}

