#include <twzthread.h>
#include <twzsys.h>

#include <limits.h>

#include <debug.h>
void twz_thread_exit(void)
{
	struct object thrd;
	twz_object_init(&thrd, TWZSLOT_THRD);
	struct twzthread_repr *repr = thrd.base;

	fbsd_thr_exit(&repr->state);
	__syscall6(1, 0, 0, 0, 0, 0, 0);
}

int twz_thread_wait(struct twzthread *th)
{
	/* TODO: optimize, define, etc */
	struct object tgt;
	twz_object_open(&tgt, th->repr, FE_READ);
	struct twzthread_repr *repr = twz_ptr_base(&tgt);
	if(repr->state == 0) {
		fbsd_sys_umtx(&repr->state, UMTX_OP_WAIT, 0);
	}
	return 0;
}

