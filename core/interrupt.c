#include <interrupt.h>
#include <thread.h>
#include <stdatomic.h>
static struct linkedlist handlers[MAX_INTERRUPT_VECTORS];
static _Atomic int initialized[MAX_INTERRUPT_VECTORS] = { false };

void interrupt_register_handler(int vector, struct interrupt_handler *handler)
{
	if(!atomic_exchange(&initialized[vector], true)) {
		linkedlist_create(&handlers[vector], 0);
	}
	linkedlist_insert(&handlers[vector], &handler->entry, handler);
}

void interrupt_unregister_handler(int vector, struct interrupt_handler *handler)
{
	assert(initialized[vector]);
	linkedlist_remove(&handlers[vector], &handler->entry);
}

void kernel_interrupt_entry(struct interrupt_frame *frame)
{
}

void kernel_interrupt_postack(struct interrupt_frame *frame)
{
	if(current_thread->flags & THREAD_SCHEDULE)
		preempt();
}

