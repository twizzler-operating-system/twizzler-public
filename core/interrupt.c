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

void kernel_interrupt_entry(void)
{
#warning "TODO"
	struct linkedlist *list = &handlers[0]; //todo
	linkedlist_lock(list);
	struct linkedentry *entry;
	for(entry = linkedlist_iter_start(list);
			entry != linkedlist_iter_end(list);
			entry = linkedlist_iter_next(entry)) {
		struct interrupt_handler *h = linkedentry_obj(entry);
		//h->fn(frame);
	}
	linkedlist_unlock(list);
}

