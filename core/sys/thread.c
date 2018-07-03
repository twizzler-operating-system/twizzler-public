#include <thread.h>
#include <syscall.h>
#include <object.h>
#include <processor.h>

long syscall_thread_spawn(__int128 foo)
{
	printk(" sys %lx %lx\n", (uint64_t)(foo >> 64), (uint64_t)(foo & 0xFFFFFFFFFFFFFFFF));
	
	return (uint64_t)(foo);
}

long syscall_thrd_ctl(int op, long arg)
{
	return 0;
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
	/* TODO: detach from scid */
	return 0;
}

