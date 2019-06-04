#include <twz/sys.h>
#include <twz/thread.h>

void twz_thread_exit(void)
{
	__seg_gs struct twzthread_repr *repr = (uintptr_t)OBJ_NULLPAGE_SIZE;
	sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->syncs[THRD_SYNC_EXIT]);
}
