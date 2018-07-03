#include <twzthread.h>
#include <twzsys.h>

#include <limits.h>

#include <debug.h>
void twz_thread_exit(void)
{
	struct object thrd;
	twz_object_init(&thrd, TWZSLOT_THRD);
	struct twzthread_repr *repr = thrd.base;

	sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->state);
}

int twz_thread_wait(struct twzthread *th)
{
	/* TODO: optimize, define, etc */
	struct object tgt;
	twz_object_open(&tgt, th->repr, FE_READ);
	struct twzthread_repr *repr = twz_ptr_base(&tgt);
	if(repr->state == 0) {
		sys_thread_sync(THREAD_SYNC_SLEEP, (int *)&repr->state, 0, NULL);
	}
	return 0;
}

