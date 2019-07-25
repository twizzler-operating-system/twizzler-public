#include <limits.h>
#include <object.h>
#include <processor.h>
#include <secctx.h>
#include <syscall.h>
#include <thread.h>

#include <twz/_thrd.h>

/* TODO (major): verify all incoming pointers from syscalls */

long syscall_thread_spawn(uint64_t tidlo,
  uint64_t tidhi,
  struct sys_thrd_spawn_args *tsa,
  int flags)
{
	objid_t tid = MKID(tidhi, tidlo);
	struct object *repr = obj_lookup(tid);
	if(!repr) {
		return -1;
	}

	spinlock_acquire_save(&repr->lock);
	if(repr->kso_type != KSO_NONE && repr->kso_type != KSO_THREAD) {
		obj_put(repr);
		return -1;
	}
	if(repr->kso_type == KSO_NONE) {
		obj_kso_init(repr, KSO_THREAD);
	}
	spinlock_release_restore(&repr->lock);

	struct object *view = current_thread ? kso_get_obj(current_thread->ctx->view, view) : NULL;
	if(tsa->target_view) {
		view = obj_lookup(tsa->target_view);
		if(view == NULL) {
			obj_put(repr);
			return -1;
		}
	}

	obj_write_data(repr, offsetof(struct twzthread_repr, reprid), sizeof(objid_t), &tid);

	struct thread *t = thread_create();
	t->thrid = tid;
	t->throbj = &repr->thr; /* krc: move */
	vm_setview(t, view);
	obj_put(view);

	if(current_thread) {
		spinlock_acquire_save(&current_thread->sc_lock);
		bool active_noi = false;
		for(int i = 0; i < MAX_SC; i++) {
			active_noi = current_thread->active_sc == current_thread->attached_scs[i]
			             && (current_thread->attached_scs_attrs[i] & TWZ_DETACH_NOINHERIT);
			if(current_thread->attached_scs[i]
			   && !(current_thread->attached_scs_attrs[i] & TWZ_DETACH_NOINHERIT)) {
				krc_get(&current_thread->attached_scs[i]->refs);
				t->attached_scs[i] = current_thread->attached_scs[i];
			}
		}
		krc_get(&current_thread->active_sc->refs);
		t->active_sc = active_noi ? NULL : current_thread->active_sc;
		spinlock_release_restore(&current_thread->sc_lock);
	} else {
		t->active_sc = secctx_alloc(0);
		krc_get(&t->active_sc->refs);
		t->attached_scs[0] = t->active_sc;
	}

	arch_thread_init(t,
	  tsa->start_func,
	  tsa->arg,
	  tsa->stack_base,
	  tsa->stack_size,
	  tsa->tls_base,
	  tsa->thrd_ctrl);

	t->state = THREADSTATE_RUNNING;
	processor_attach_thread(NULL, t);

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
			/* TODO (sec): check eptr */
			eptr = (long *)arg;
			if(eptr) {
				thread_sync_single(THREAD_SYNC_WAKE, eptr, INT_MAX, NULL);
			}
			thread_exit();
			break;
		default:
			ret = -1;
	}
	return ret;
}

long syscall_become(uint64_t sclo, uint64_t schi, struct arch_syscall_become_args *_ba)
{
	objid_t scid = MKID(schi, sclo);
	struct arch_syscall_become_args ba;
	memcpy(&ba, _ba, sizeof(ba));
	if(ba.target_view) {
		struct object *target_view = obj_lookup(ba.target_view);
		if(!target_view) {
			return -1;
		}
		vm_setview(current_thread, target_view);
		arch_mm_switch_context(current_thread->ctx);
		obj_put(target_view);
	}
	arch_thread_become(&ba);
	syscall_detach(0, 0, sclo, schi, 0);
	secctx_become_detach(current_thread);
	return 0;
}
