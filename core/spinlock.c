#include <spinlock.h>
#include <interrupt.h>
#include <stdatomic.h>
#include <processor.h>
#include <instrument.h>

bool spinlock_acquire(struct spinlock *lock)
{
	register bool set = arch_interrupt_set(0);
	while(atomic_fetch_or_explicit(&lock->data, 1, memory_order_acquire) & 1) {
		while(atomic_load_explicit(&lock->data, memory_order_acquire))
			arch_processor_relax();
	}
	return set;
}

void spinlock_release(struct spinlock *lock, bool flags)
{
	atomic_store_explicit(&lock->data, 0, memory_order_release);
	arch_interrupt_set(flags);
}

