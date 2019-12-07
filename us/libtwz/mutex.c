#include <twz/debug.h>
#include <twz/mutex.h>

_Atomic uint64_t _twz_rcode = 0;
void mutex_acquire(struct mutex *m)
{
	if(!_twz_rcode) {
		uint64_t r = sys_kconf(KCONF_RDRESET, 0);
		_twz_rcode = r;
	}

	if(m->resetcode != _twz_rcode) {
		/* might need to bust the lock. Make sure everyone still comes in here until we're done by
		 * changing the resetcode to -1. If we exchange and get 1, we wait. If we don't get -1,
		 * then we're the one that's going to bust the lock. */
		if(atomic_exchange(&m->resetcode, ~0ul) != ~0ul) {
			/* bust lock, and then store new reset code */
			atomic_store(&m->sleep, 0);
			atomic_store(&m->resetcode, _twz_rcode);
		} else {
			while(atomic_load(&m->resetcode) == ~0ul)
				asm("pause");
		}
	}
	long v;
	for(int i = 0; i < 100; i++) {
		v = 0;
		if(atomic_compare_exchange_strong(&m->sleep, &v, 1)) {
			return;
		}
		asm("pause");
	}
	if(v)
		v = atomic_exchange(&m->sleep, 2);

	struct sys_thread_sync_args args = {
		.op = THREAD_SYNC_SLEEP,
		.addr = (uint64_t *)&m->sleep,
		.arg = 2,
	};

	while(v) {
		sys_thread_sync(1, &args);

		v = atomic_exchange(&m->sleep, 2);
	}
}

void mutex_release(struct mutex *m)
{
	if(atomic_load(&m->sleep) == 2) {
		atomic_store(&m->sleep, 0);
	} else if(atomic_exchange(&m->sleep, 0) == 1) {
		return;
	}

	for(int i = 0; i < 100; i++) {
		long v = 1;
		if(atomic_load(&m->sleep)) {
			if(atomic_compare_exchange_strong(&m->sleep, &v, 2))
				return;
		}
		asm("pause");
	}
	struct sys_thread_sync_args args = {
		.op = THREAD_SYNC_WAKE,
		.addr = (uint64_t *)&m->sleep,
		.arg = 1,
	};
	sys_thread_sync(1, &args);
}
