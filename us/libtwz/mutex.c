#include <mutex.h>
void mutex_acquire(struct mutex *m)
{
	long v;
	for(int i=0;i<100;i++) {
		v = 0;
		if(atomic_compare_exchange_strong(&m->sleep, &v, 1)) {
			return;
		}
		asm("pause");
	}
	if(v) v = atomic_exchange(&m->sleep, 2);
	while(v) {
		fbsd_sys_umtx(&m->sleep, UMTX_OP_WAIT, 2);
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
		long v = 1;
		if(atomic_load(&m->sleep)) {
			if(atomic_compare_exchange_strong(&m->sleep, &v, 2)) return;
		}
		asm("pause");
	}
	fbsd_sys_umtx(&m->sleep, UMTX_OP_WAKE, 1);
}


