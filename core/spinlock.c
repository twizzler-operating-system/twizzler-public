#include <instrument.h>
#include <interrupt.h>
#include <processor.h>
#include <spinlock.h>
#include <stdatomic.h>

bool __spinlock_acquire(struct spinlock *lock, const char *f __unused, int l __unused)
{
	register bool set = arch_interrupt_set(0);
	__unused size_t count = 0;
	while(atomic_fetch_or_explicit(&lock->data, 1, memory_order_acquire) & 1) {
		while(atomic_load_explicit(&lock->data, memory_order_acquire)) {
			arch_processor_relax();
#if CONFIG_DEBUG_LOCKS
			if(count++ == 100000000ul && f) {
				panic("POTENTIAL DEADLOCK in cpu %ld trying to acquire %s:%d (held from %s:%d by "
				      "cpu %ld)\n",
				  current_thread ? (long)current_thread->processor->id : -1,
				  f,
				  l,
				  lock->holder_file,
				  lock->holder_line,
				  lock->holder_thread ? (long)lock->holder_thread->processor->id : -1);
			}
#endif
		}
	}
#if CONFIG_DEBUG_LOCKS
	lock->holder_file = f;
	lock->holder_line = l;
	lock->holder_thread = current_thread;
#endif
	return set;
}

void __spinlock_release(struct spinlock *lock, bool flags, const char *f, int l)
{
	(void)f;
	(void)l;
	/* TODO: when not debugging locks, dont have these arguments */
#if CONFIG_DEBUG_LOCKS
	lock->holder_file = NULL;
	lock->holder_line = 0;
	lock->holder_thread = NULL;
#endif
	atomic_store_explicit(&lock->data, 0, memory_order_release);
	arch_interrupt_set(flags);
}
