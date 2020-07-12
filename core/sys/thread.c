#include <limits.h>
#include <object.h>
#include <processor.h>
#include <secctx.h>
#include <syscall.h>
#include <thread.h>

#include <twz/_sctx.h>
#include <twz/_thrd.h>

long syscall_thread_spawn(uint64_t tidlo,
  uint64_t tidhi,
  struct sys_thrd_spawn_args *tsa,
  int flags)
{
	if(current_thread && !verify_user_pointer(tsa, sizeof(*tsa))) {
		return -EINVAL;
	}
	void *start = tsa->start_func;
	void *stack_base = tsa->stack_base;
	void *tls_base = tsa->tls_base;
	if(!verify_user_pointer(start, sizeof(void *))
	   || !verify_user_pointer(stack_base, sizeof(void *))
	   || !verify_user_pointer(tls_base, sizeof(void *))) {
		return -EINVAL;
	}

	if(flags) {
		return -EINVAL;
	}
	objid_t tid = MKID(tidhi, tidlo);
	struct object *repr = obj_lookup(tid, 0);
	if(!repr) {
		return -ENOENT;
	}

	int r;
	if((r = obj_check_permission(repr, SCP_WRITE))) {
		obj_put(repr);
		return r;
	}

	spinlock_acquire_save(&repr->lock);
	if(repr->kso_type != KSO_NONE && repr->kso_type != KSO_THREAD) {
		obj_put(repr);
		return -EINVAL;
	}
	if(repr->kso_type == KSO_NONE) {
		obj_kso_init(repr, KSO_THREAD);
	}
	spinlock_release_restore(&repr->lock);

	struct object *view;
	if(tsa->target_view) {
		view = obj_lookup(tsa->target_view, 0);
		if(view == NULL) {
			obj_put(repr);
			return -ENOENT;
		}
		/* TODO (sec): shoud this be SCP_USE? */
		if((r = obj_check_permission(view, SCP_WRITE))) {
			obj_put(view);
			obj_put(repr);
			return r;
		}
	} else {
		view = kso_get_obj(current_thread->ctx->view, view);
	}

	obj_write_data(repr, offsetof(struct twzthread_repr, reprid), sizeof(objid_t), &tid);

	struct thread *t = thread_create();
	t->thrid = tid;
	t->throbj = &repr->thr; /* krc: move */
	repr->thr.thread = t;
	vm_setview(t, view);

	obj_put(view);

	t->kso_attachment_num = kso_root_attach(repr, 0, KSO_THREAD);

	if(current_thread) {
		spinlock_acquire_save(&current_thread->sc_lock);
		for(int i = 0; i < MAX_SC; i++) {
			if(current_thread->sctx_entries[i].context) {
				krc_get(&current_thread->sctx_entries[i].context->refs);
				t->sctx_entries[i].context = current_thread->sctx_entries[i].context;
				t->sctx_entries[i].attr = 0;
				t->sctx_entries[i].backup_attr = 0;
			}
		}
		krc_get(&current_thread->active_sc->refs);
		t->active_sc = current_thread->active_sc;
		spinlock_release_restore(&current_thread->sc_lock);
	} else {
		t->active_sc = secctx_alloc(0);
		t->active_sc->superuser = true; /* we're the init thread */
		krc_get(&t->active_sc->refs);
		t->sctx_entries[0].context = t->active_sc;
	}

	arch_thread_init(t, start, tsa->arg, stack_base, tsa->stack_size, tls_base, tsa->thrd_ctrl);

	t->state = THREADSTATE_RUNNING;
	processor_attach_thread(NULL, t);
	// printk("spawned thread %ld from %ld on processor %d\n",
	// t->id,
	// current_thread ? (long)current_thread->id : -1,
	// t->processor->id);

	return 0;
}

long syscall_thrd_ctl(int op, long arg)
{
	if(op <= THRD_CTL_ARCH_MAX) {
		return arch_syscall_thrd_ctl(op, arg);
	}
	int ret;
	switch(op) {
		long *eptr;
		case THRD_CTL_EXIT:
			eptr = (long *)arg;
			if(eptr && verify_user_pointer(eptr, sizeof(void *))) {
				thread_sync_single(THREAD_SYNC_WAKE, eptr, INT_MAX, false);
			}
			thread_exit();
			break;
		default:
			ret = -EINVAL;
	}
	return ret;
}

long syscall_become(struct arch_syscall_become_args *_ba)
{
	if(!verify_user_pointer(_ba, sizeof(*_ba)))
		return -EINVAL;
	struct arch_syscall_become_args ba;
	memcpy(&ba, _ba, sizeof(ba));
	if(ba.target_view) {
		struct object *target_view = obj_lookup(ba.target_view, 0);
		if(!target_view) {
			return -ENOENT;
		}
		int r;
		if((r = obj_check_permission(target_view, SCP_WRITE))) {
			obj_put(target_view);
			return r;
		}

		vm_setview(current_thread, target_view);

		arch_mm_switch_context(current_thread->ctx);
		obj_put(target_view);
	}
	arch_thread_become(&ba);
	return 0;
}
