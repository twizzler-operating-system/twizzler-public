#include <twz/_slots.h>
#include <twz/_view.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/thread.h>

int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args)
{
	__seg_gs struct twzthread_repr *currepr = (uintptr_t)OBJ_NULLPAGE_SIZE;
	int r;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &thrd->tid))) {
		return r;
	}
	if((r = twz_object_open(&thrd->obj, thrd->tid, FE_READ | FE_WRITE))) {
		return r;
	}

	struct twzthread_repr *newrepr = twz_obj_base(&thrd->obj);

	newrepr->reprid = thrd->tid;
	for(size_t i = 0; i < NUM_FAULTS; i++) {
		newrepr->faults[i] = currepr->faults[i];
	}

	newrepr->fixed_points[TWZSLOT_THRD] = (struct viewentry){
		.id = thrd->tid,
		.flags = VE_READ | VE_WRITE | VE_VALID,
	};

	struct sys_thrd_spawn_args sa = {
		.target_view = args->target_view,
		.start_func = args->start_func,
		.arg = args->arg,
		.stack_base = args->stack_base,
		.stack_size = args->stack_size,
		.tls_base = args->tls_base,
	};
	if(!args->stack_base) {
		objid_t sid;
		if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &sid))) {
			return r;
		}

		newrepr->fixed_points[TWZSLOT_STACK] = (struct viewentry){
			.id = sid,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};
		sa.stack_base = (char *)SLOT_TO_VADDR(TWZSLOT_STACK) + OBJ_NULLPAGE_SIZE;
		sa.stack_size = TWZ_THREAD_STACK_SIZE;
		sa.tls_base =
		  (char *)SLOT_TO_VADDR(TWZSLOT_STACK) + OBJ_NULLPAGE_SIZE + TWZ_THREAD_STACK_SIZE;
	}

	return sys_thrd_spawn(thrd->tid, &sa, 0);
}

void twz_thread_exit(void)
{
	__seg_gs struct twzthread_repr *repr = (uintptr_t)OBJ_NULLPAGE_SIZE;
	sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->syncs[THRD_SYNC_EXIT]);
}
