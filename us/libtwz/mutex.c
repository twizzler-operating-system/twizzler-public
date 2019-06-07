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
	while(v) {
		sys_thread_sync(1,
		  (int[1]){ THREAD_SYNC_SLEEP },
		  (long * [1]){ (long *)&m->sleep },
		  (long[1]){ 2 },
		  NULL,
		  NULL);

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
	sys_thread_sync(1,
	  (int[1]){ THREAD_SYNC_WAKE },
	  (long * [1]){ (long *)&m->sleep },
	  (long[1]){ 1 },
	  NULL,
	  NULL);
}
