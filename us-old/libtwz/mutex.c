#include <mutex.h>
void mutex_acquire(struct mutex *m)
{
	int v;
	for(int i=0;i<100;i++) {
		v = 0;
		if(atomic_compare_exchange_strong(&m->sleep, &v, 1)) {
			return;
		}
		asm("pause");
	}
	if(v) v = atomic_exchange(&m->sleep, 2);
	while(v) {
		sys_thread_sync(THREAD_SYNC_SLEEP, (int *)&m->sleep, 2, NULL);
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

	for(int i=0;i<100;i++) {
		int v = 1;
		if(atomic_load(&m->sleep)) {
			if(atomic_compare_exchange_strong(&m->sleep, &v, 2)) return;
		}
		asm("pause");
	}
	sys_thread_sync(THREAD_SYNC_WAKE, (int *)&m->sleep, 1, NULL);
}


