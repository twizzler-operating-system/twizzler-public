#include <interrupt.h>
#include <lib/iter.h>
#include <stdatomic.h>
#include <thread.h>
static struct list handlers[MAX_INTERRUPT_VECTORS];
static struct spinlock locks[MAX_INTERRUPT_VECTORS] = { [0 ... MAX_INTERRUPT_VECTORS - 1] =
	                                                      SPINLOCK_INIT };
static bool initialized[MAX_INTERRUPT_VECTORS] = { false };

void interrupt_register_handler(int vector, struct interrupt_handler *handler)
{
	spinlock_acquire_save(&locks[vector]);
	if(!initialized[vector]) {
		list_init(&handlers[vector]);
		initialized[vector] = true;
	}
	list_insert(&handlers[vector], &handler->entry);
	arch_interrupt_unmask(vector);
	spinlock_release_restore(&locks[vector]);
}

void interrupt_unregister_handler(int vector, struct interrupt_handler *handler)
{
	assert(initialized[vector]);
	spinlock_acquire_save(&locks[vector]);
	list_remove(&handler->entry);
	if(list_empty(&handlers[vector]))
		arch_interrupt_mask(vector);
	spinlock_release_restore(&locks[vector]);
}

void kernel_interrupt_entry(int vector)
{
	struct list *list = &handlers[vector];
	if(!initialized[vector] || list_empty(list)) {
		return;
	}
	bool fl = spinlock_acquire(&locks[vector]);
	foreach(entry, list, list) {
		struct interrupt_handler *h = list_entry(entry, struct interrupt_handler, entry);
		h->fn(vector, h);
	}
	spinlock_release(&locks[vector], fl);
}
