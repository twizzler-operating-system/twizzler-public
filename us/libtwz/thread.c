#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <twz/_err.h>
#include <twz/_slots.h>
#include <twz/_types.h>
#include <twz/_view.h>
#include <twz/debug.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/thread.h>

void *__copy_tls(char *);

struct twzthread_repr *twz_thread_repr_base(void)
{
	uint64_t a;
	asm volatile("rdgsbase %%rax" : "=a"(a));
	if(!a) {
		libtwz_panic("could not find twz_thread_repr_base");
	}
	return (struct twzthread_repr *)(a + OBJ_NULLPAGE_SIZE);
}

int twz_thread_release(struct thread *thrd)
{
	if(thrd->tid == 0) {
		return -EINVAL;
	}
	thrd->tid = 0;
	int r;
	if((r = twz_object_unwire(NULL, &thrd->obj))) {
		libtwz_panic("failed to unwire thread object during thread release: %s\n", strerror(-r));
	}
	twz_object_release(&thrd->obj);
	return 0;
}

twzobj *__twz_get_stdstack_obj(void)
{
	static twzobj *_Atomic obj = NULL;
	static _Atomic int x = 0;
	if(!obj) {
		while(atomic_exchange(&x, 1))
			;
		if(!obj) {
			obj = malloc(sizeof(*obj));
			*obj = twz_object_from_ptr(SLOT_TO_VADDR(TWZSLOT_STACK));
		}
		x = 0;
	}
	return obj;
}

int twz_thread_create(struct thread *thrd)
{
	int r;
	struct twzthread_repr *currepr = twz_thread_repr_base();

	if((r = twz_object_new(
	      &thrd->obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_VIEW))) {
		goto error;
	}

	struct twzthread_repr *newrepr = twz_object_base(&thrd->obj);

	newrepr->reprid = thrd->tid = twz_object_guid(&thrd->obj);
	for(size_t i = 0; i < NUM_FAULTS; i++) {
		newrepr->faults[i] = currepr->faults[i];
	}

	newrepr->fixed_points[TWZSLOT_THRD] = (struct viewentry){
		.id = thrd->tid,
		.flags = VE_READ | VE_WRITE | VE_VALID,
	};
	return 0;

error:
	twz_object_delete(&thrd, 0);
	twz_object_release(&thrd->obj);
	return r;
}

int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args)
{
	int r;
	if((r = twz_thread_create(thrd))) {
		return r;
	}

	struct twzthread_repr *currepr = twz_thread_repr_base();
	struct twzthread_repr *newrepr = twz_object_base(&thrd->obj);

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
		twzobj stack;
		/* if we weren't provided a stack, make a new one tied to the thread object */
		if((r = twz_object_new(&stack,
		      NULL,
		      NULL,
		      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
			twz_thread_release(thrd);
			return r;
		}

		if((r = twz_object_tie(&thrd->obj, &stack, 0))) {
			if(twz_object_delete(&stack, 0)) {
				libtwz_panic("failed to delete object during cleanup");
			}
			twz_thread_release(thrd);
			return r;
		}

		if(twz_object_delete(&stack, 0)) {
			libtwz_panic("failed to delete stack object during thread spawn");
		}

		newrepr->fixed_points[TWZSLOT_STACK] = (struct viewentry){
			.id = twz_object_guid(&stack),
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};
		sa.stack_base = (char *)SLOT_TO_VADDR(TWZSLOT_STACK) + OBJ_NULLPAGE_SIZE;
		sa.stack_size = TWZ_THREAD_STACK_SIZE;
		sa.tls_base =
		  (char *)SLOT_TO_VADDR(TWZSLOT_STACK) + OBJ_NULLPAGE_SIZE + TWZ_THREAD_STACK_SIZE;

		/* TODO: can we reduce these permissions? */
		r = twz_ptr_store_guid(&stack, &sa.arg, NULL, args->arg, FE_READ | FE_WRITE);
		if(r) {
			/* stack is already deleted, and tied to thread */
			twz_thread_release(thrd);
			return r;
		}
	} else {
		newrepr->fixed_points[TWZSLOT_STACK] = (struct viewentry){
			.id = currepr->fixed_points[TWZSLOT_STACK].id,
			.flags = VE_READ | VE_WRITE | VE_VALID,
		};
	}

	if((r = sys_thrd_spawn(thrd->tid, &sa, 0) < 0)) {
		twz_thread_release(thrd);
	}

	return r;
}

void twz_thread_exit(uint64_t ecode)
{
	struct twzthread_repr *repr = twz_thread_repr_base();
	repr->syncinfos[THRD_SYNC_EXIT] = ecode;
	repr->syncs[THRD_SYNC_EXIT] = 1;
	sys_thrd_ctl(THRD_CTL_EXIT, (long)&repr->syncs[THRD_SYNC_EXIT]);
	__builtin_unreachable();
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
		struct sys_thread_sync_args args[count];
		for(size_t i = 0; i < count; i++) {
			struct twzthread_repr *r = twz_object_base(&threads[i]->obj);

			args[i].addr = (uint64_t *)&r->syncs[syncpoints[i]];
			args[i].arg = 0;
			args[i].op = THREAD_SYNC_SLEEP;
			args[i].flags = 0;
			if(r->syncs[syncpoints[i]]) {
				if(event)
					event[i] = 1;
				if(info)
					info[i] = r->syncinfos[syncpoints[i]];
				ready++;
			}
		}
		if(!ready) {
			int r = sys_thread_sync(count, args);
			if(r < 0)
				return r;
		}
	} while(ready == 0);
	return ready;
}

#include <limits.h>
int twz_thread_ready(struct thread *thread, int sp, uint64_t info)
{
	struct twzthread_repr *repr;
	if(thread) {
		repr = twz_object_base(&thread->obj);
	} else {
		repr = twz_thread_repr_base();
	}

	repr->syncinfos[sp] = info;
	repr->syncs[sp] = 1;
	struct sys_thread_sync_args args = {
		.op = THREAD_SYNC_WAKE,
		.addr = (uint64_t *)&repr->syncs[sp],
		.arg = UINT64_MAX,
	};
	return sys_thread_sync(1, &args);
}

void twz_thread_sync_init(struct sys_thread_sync_args *args,
  int op,
  _Atomic uint64_t *addr,
  uint64_t val,
  struct timespec *timeout)
{
	*args = (struct sys_thread_sync_args){
		.op = op,
		.addr = (uint64_t *)addr,
		.arg = val,
		.spec = timeout,
	};
	if(timeout)
		args->flags |= THREAD_SYNC_TIMEOUT;
}
int twz_thread_sync(int op, _Atomic uint64_t *addr, uint64_t val, struct timespec *timeout)
{
	struct sys_thread_sync_args args = {
		.op = op,
		.addr = (uint64_t *)addr,
		.arg = val,
		.spec = timeout,
	};
	if(timeout)
		args.flags |= THREAD_SYNC_TIMEOUT;
	return sys_thread_sync(1, &args);
}

int twz_thread_sync_multiple(size_t c, struct sys_thread_sync_args *args)
{
	return sys_thread_sync(c, args);
}
