#include <thread.h>
#include <syscall.h>
#include <object.h>
#include <processor.h>
#include <limits.h>

long syscall_thread_spawn(uint64_t tidlo, uint64_t tidhi,
		struct sys_thrd_spawn_args *tsa, int flags)
{
	return 0;
}

long syscall_thrd_ctl(int op, long arg)
{
	if(op <= THRD_CTL_ARCH_MAX) {
		return arch_syscall_thrd_ctl(op, arg);
	}
	int ret;
	switch(op) {
		int *eptr;
		case THRD_CTL_EXIT:
			/* TODO (sec): check eptr */
			eptr = (int *)arg;
			if(eptr) {
				*eptr = 1;
				syscall_thread_sync(THREAD_SYNC_WAKE, eptr, INT_MAX, NULL);
			}
			thread_exit();
			break;
		default:
			ret = -1;
	}
	return ret;
}

long syscall_become(uint64_t sclo, uint64_t schi, struct arch_syscall_become_args *ba)
{
	objid_t scid = MKID(schi, sclo);
	struct object *target_view = obj_lookup(ba->target_view);
	if(!target_view) {
		return -1;
	}
	arch_thread_become(ba);
	vm_setview(current_thread, target_view);
	obj_put(target_view);
	syscall_detach(0, 0, sclo, schi, 0);
	return 0;
}

