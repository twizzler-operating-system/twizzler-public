#include <mutex.h>
#include <stdatomic.h>
#include <debug.h>
#include <thread.h>
#include <processor.h>
#include <interrupt.h>
static inline bool _spinlock_first(struct mutex *mutex)
{
	/* improve performance with an optimistic view that the mutex
	 * might get unlocked soon by a thread on a different CPU */
	for(int i=0;i<40;i++) { /* also 40 is arbitrary */
		int exp = 0;
		if(atomic_compare_exchange_weak(&mutex->lock, &exp, 1))
			return true;
		/* ...and if that doesn't look like it's working, maybe
		 * it will get unlocked soon by a thread on the same CPU,
		 * so we can get away with rescheduling until it unlocks it */
		if(i > 30)
			schedule();
	}
	/* ...neither of those worked, so indicate that we couldn't grab
	 * the mutex by spinning, and block. */
	return false;
}

void mutex_acquire(struct mutex *mutex)
{
#if CONFIG_DEBUG
	assert(!current_thread || mutex->owner != current_thread);
#endif
	if(!current_thread)
		return (void)atomic_fetch_add(&mutex->lock, 1);
	if(!_spinlock_first(mutex)) {
		struct blockpoint bp;
		blockpoint_create(&bp, BLOCKPOINT_UNINTERRUPT);

		blocklist_attach(&mutex->wait, &bp);
		if(atomic_fetch_add(&mutex->lock, 1) > 0) {
			schedule();
		}
		enum block_result res = blockpoint_cleanup(&bp);
		assert(res == BLOCKRES_UNBLOCKED || res == BLOCKRES_BLOCKED);
	}
#if CONFIG_DEBUG
	assert(mutex->owner == NULL);
	mutex->owner = current_thread;
#endif
}

void mutex_release(struct mutex *mutex)
{
#if CONFIG_DEBUG
	assert(!current_thread || mutex->owner == current_thread);
	mutex->owner = NULL;
#endif
	if(!current_thread)
		return (void)atomic_fetch_sub(&mutex->lock, 1);
	if(atomic_fetch_sub(&mutex->lock, 1) != 1) {
		blocklist_wake(&mutex->wait, 1);
	}
}

void mutex_create(struct mutex *mutex)
{
	mutex->lock = ATOMIC_VAR_INIT(0);
	blocklist_create(&mutex->wait);
}

