#include <spinlock.h>
#include <interrupt.h>
#include <stdatomic.h>
#include <processor.h>

void spinlock_acquire(struct spinlock *lock)
{
	bool set = arch_interrupt_set(0);

	while(atomic_fetch_or(&lock->data, 1) & 1) {
		arch_processor_relax();
	}
	
	lock->data |= set ? (1 << 1) : 0;
}

void spinlock_release(struct spinlock *lock)
{
	bool set = !!(lock->data & (1 << 1));
	atomic_store(&lock->data, 0);
	arch_interrupt_set(set);
}

