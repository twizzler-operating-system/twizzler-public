#pragma once

#include <twzobj.h>
#include <twzview.h>
#include <twzsys.h>

struct twzthread {
	long tid;
	objid_t repr;
	unsigned int flags;
};

#define TLS_SIZE 0x200000
#define STACK_SIZE 0x200000
#define STACK_BASE SLOT_TO_VIRT(TWZSLOT_THRD)
#define TLS_BASE (SLOT_TO_VIRT(TWZSLOT_THRD) + STACK_SIZE)

struct twzthread_repr {
	unsigned char stack[STACK_SIZE];
	unsigned char tls[STACK_SIZE];
	objid_t reprid;
	_Atomic int state;
};

#define TWZTHR_SPAWN_ARGWRITE 1

static inline int twz_thread_spawn(struct twzthread *t,
		void (*entry)(void *), struct object *obj, void *arg, int flags)
{
	objid_t reprid = 0;
	int ret;
	struct object thrd;
	if((ret = twz_object_new(&thrd, &reprid, 0, 0,
					KSO_TYPE(KSO_THRD)
					| TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE | TWZ_ON_DFL_USE)) != 0) {
		return ret;
	}

	void *aptr;
	if((ret = twz_ptr_swizzle(&thrd, &aptr, obj, arg,
			FE_READ | (flags & TWZTHR_SPAWN_ARGWRITE ? FE_WRITE : 0))) < 0) {
		return ret;
	}

	t->repr = reprid;
	t->flags = flags;
	struct twzthread_repr *reprstruct = thrd.base;
	reprstruct->reprid = reprid;
	reprstruct->state = 0;
	
	struct sys_thrd_spawn_args param = {
		.start_func = entry,
		.arg        = aptr,
		.stack_base = STACK_BASE,
		.stack_size = STACK_SIZE,
		.tls_base   = TLS_BASE,
	};
	return sys_thrd_spawn(reprid, &param, 0);
}

static inline int twz_thread_become(objid_t sid, objid_t vid, void *jmp, int flags __unused)
{
	struct sys_become_args ba = {
		.target_view = vid,
		.target_rip = (long)jmp,
	};
	return sys_become(sid, &ba);
}

void twz_thread_exit(void);
int twz_thread_wait(struct twzthread *th);

