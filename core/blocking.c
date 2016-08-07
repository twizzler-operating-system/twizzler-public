#include <thread.h>
#include <processor.h>
#include <guard.h>

void blocklist_create(struct blocklist *bl)
{
	linkedlist_create(&bl->list, LINKEDLIST_LOCKLESS);
	bl->lock = SPINLOCK_INIT;
}

void blockpoint_create(struct blockpoint *bp)
{
	bp->thread = current_thread;
	bp->flags = 0;
}

void blocklist_attach(struct blocklist *bl, struct blockpoint *bp)
{
	processor_disable_preempt();
	spinlock_acquire(&bl->lock);
	linkedlist_insert(&bl->list, &bp->entry, bp);
	bp->blocklist = bl;
	bp->result = BLOCKRES_BLOCKED;
	current_thread->state = THREADSTATE_BLOCKED;
	spinlock_release(&bl->lock);
}

/* n=0 for wake all */
void blocklist_wake(struct blocklist *bl, int n)
{
	spinlock_guard(&bl->lock);
	struct linkedentry *entry;
	for(entry = linkedlist_iter_start(&bl->list);
			entry != linkedlist_iter_end(&bl->list);
			entry = linkedlist_iter_next(entry)) {
		struct blockpoint *bp = linkedentry_obj(entry);
		enum block_result exp = BLOCKRES_BLOCKED;
		if(atomic_compare_exchange_strong(&bp->result, &exp, BLOCKRES_UNBLOCKED)) {
			bp->thread->state = THREADSTATE_RUNNING;
			processor_attach_thread(bp->thread->processor, bp->thread);
			/* if called with n = 0, this will never be true */
			if(--n == 0) {
				return;
			}
		}
	}
}

enum block_result blockpoint_cleanup(struct blockpoint *bp)
{
	struct blocklist *bl = bp->blocklist;
	spinlock_acquire(&bl->lock);
	linkedlist_remove(&bl->list, &bp->entry);
	spinlock_release(&bl->lock);
	processor_enable_preempt();
	return bp->result;
}

