#include <interrupt.h>
#include <lib/iter.h>
#include <stdatomic.h>
#include <thread.h>
static struct list handlers[MAX_INTERRUPT_VECTORS];
static struct spinlock locks[MAX_INTERRUPT_VECTORS] = { [0 ... MAX_INTERRUPT_VECTORS - 1] =
	                                                      SPINLOCK_INIT };
static bool initialized[MAX_INTERRUPT_VECTORS] = { false };

static struct spinlock alloc_lock = SPINLOCK_INIT;

int interrupt_allocate_vectors(size_t count, struct interrupt_alloc_req *req)
{
	/* TODO: arch-dep */
	static size_t vp = 64;
	bool ok = true;
	spinlock_acquire_save(&alloc_lock);
	for(size_t i = 0; i < count; i++, req++) {
		if(!(req->flags & INTERRUPT_ALLOC_REQ_VALID) || (req->flags & INTERRUPT_ALLOC_REQ_ENABLED))
			continue;
		size_t ov = vp - 1;
		req->vec = -1;
		for(; vp != ov && req->vec == -1;) {
			spinlock_acquire_save(&locks[vp]);
			if(initialized[vp] && !list_empty(&handlers[vp]) && req->pri == IVP_UNIQUE)
				continue;
			if(!initialized[vp]) {
				list_init(&handlers[vp]);
				initialized[vp] = true;
			}
			list_insert(&handlers[vp], &req->handler.entry);
			arch_interrupt_unmask(vp);

			req->vec = vp;
			req->flags |= INTERRUPT_ALLOC_REQ_ENABLED;
			spinlock_release_restore(&locks[vp]);
			vp++;
			if(vp >= MAX_INTERRUPT_VECTORS)
				vp = 64;
		}
		if(req->vec == -1)
			ok = false;
	}
	spinlock_release_restore(&alloc_lock);
	return ok ? 0 : -1;
}

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
