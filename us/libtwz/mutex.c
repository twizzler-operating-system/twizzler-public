#include <twz/debug.h>
#include <twz/mutex.h>
void mutex_acquire(struct mutex *m)
{
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
