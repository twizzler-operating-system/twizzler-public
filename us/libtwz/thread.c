#include <string.h>
#include <twz/_err.h>
#include <twz/_slots.h>
#include <twz/_types.h>
#include <twz/_view.h>
#include <twz/debug.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/thread.h>

int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args)
{
	struct twzthread_repr *currepr = twz_thread_repr_base();
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
		.thrd_ctrl = TWZSLOT_THRD,
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
	//__seg_gs struct twzthread_repr *repr = (uintptr_t)OBJ_NULLPAGE_SIZE;
	struct twzthread_repr *repr = twz_thread_repr_base();
	sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->syncs[THRD_SYNC_EXIT]);
}

ssize_t twz_thread_wait(size_t count,
  struct thread **threads,
  int *syncpoints,
  long *event,
  uint64_t *info)
{
	if(count == 0)
		return 0;
	if(count > 4096)
		return -EINVAL;

	size_t ready;
	do {
		ready = 0;
		long *addrs[count];
		int ops[count];
		for(size_t i = 0; i < count; i++) {
			struct twzthread_repr *r = twz_obj_base(&threads[i]->obj);

			addrs[i] = (long *)&r->syncs[syncpoints[i]];
			ops[i] = THREAD_SYNC_SLEEP;
			if(r->syncs[syncpoints[i]]) {
				if(event)
					event[i] = 1;
				if(info)
					info[i] = r->syncinfos[syncpoints[i]];
				ready++;
			}
		}
		if(!ready) {
			long z[count];
			memset(z, 0, sizeof(z));

			int r = sys_thread_sync(count, ops, addrs, z, event, NULL);
			if(r < 0)
				return r;
		}
	} while(ready == 0);
	return ready;
}

int twz_thread_ready(struct thread *thread, int sp, uint64_t info)
{
	struct twzthread_repr *repr;
	if(thread) {
		repr = twz_obj_base(&thread->obj);
	} else {
		repr = twz_thread_repr_base();
	}

	repr->syncinfos[sp] = info;
	repr->syncs[sp] = 1;
	return sys_thread_sync(1,
	  (int[]){ THREAD_SYNC_WAKE },
	  (long *[]){ (long *)&repr->syncs[sp] },
	  (long[]){ -1 },
	  NULL,
	  NULL);
}
