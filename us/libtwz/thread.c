#include <twzthread.h>
#include <twzsys.h>

#include <limits.h>

#include <debug.h>

struct object stdobj_thrd = TWZ_OBJECT_INIT(TWZSLOT_THRD);

void twz_thread_exit(void)
{
	struct twzthread_repr *repr = twz_ptr_base(&stdobj_thrd);
	sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->state);
}

int twz_thread_wait(struct twzthread *th)
{
	struct object tgt;
	twz_object_open(&tgt, th->repr, FE_READ);
	struct twzthread_repr *repr = twz_ptr_base(&tgt);
	if(repr->state == 0) {
		return sys_thread_sync(THREAD_SYNC_SLEEP, (int *)&repr->state, 0, NULL);
	}
	return 0;
}

int twz_thread_wait_ready(struct twzthread *th)
{
	struct object tgt;
	twz_object_open(&tgt, th->repr, FE_READ);
	struct twzthread_repr *repr = twz_ptr_base(&tgt);
	if(repr->ready == 0) {
		sys_thread_sync(THREAD_SYNC_SLEEP, (int *)&repr->ready, 0, NULL);
	}
	return 0;
}

int twz_thread_ready(void)
{
	struct twzthread_repr *repr = twz_ptr_base(&stdobj_thrd);
	repr->ready = 1;
	return sys_thread_sync(THREAD_SYNC_WAKE, (int *)&repr->ready, INT_MAX, NULL);
}

