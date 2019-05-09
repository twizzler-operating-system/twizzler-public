#pragma once

#include <twzobj.h>
#include <twzview.h>
#include <twzsys.h>

struct twzthread {
	long tid;
	objid_t repr;
	unsigned int flags;
};

enum {
	FAULT_OBJECT,
	FAULT_NULL,
	FAULT_EXCEPTION,
	NUM_FAULTS,
};

struct faultinfo {
	objid_t view;
	void *addr;
	uint64_t flags;
} __packed;

#define STACK_SIZE ( 0x200000 - OBJ_NULLPAGE_SIZE )
#define TLS_SIZE   0x200000

struct twzthread_repr {
	union {
		struct {
			struct faultinfo faults[NUM_FAULTS];
			objid_t reprid;
			char pad[256];
		} thread_kso_data;
		char _pad[4096];
	};
	unsigned char stack[STACK_SIZE];
	unsigned char tls[TLS_SIZE];
	unsigned char exec_data[4096];
	_Atomic int state, ready;
};

#define STACK_BASE ({ SLOT_TO_VIRT(TWZSLOT_THRD) + OBJ_NULLPAGE_SIZE + offsetof(struct twzthread_repr, stack); })
#define TLS_BASE ({ STACK_BASE + STACK_SIZE; })

extern struct object stdobj_thrd;

#define TWZTHR_SPAWN_ARGWRITE 1

#include <debug.h>
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
	struct twzthread_repr *my_reprstruct = stdobj_thrd.base;
	reprstruct->state = 0;
	reprstruct->ready = 0;

	memcpy(reprstruct->thread_kso_data.faults, my_reprstruct->thread_kso_data.faults,
			sizeof(struct faultinfo) * NUM_FAULTS);

	struct sys_thrd_spawn_args param = {
		.start_func = entry,
		.arg        = aptr,
		.stack_base = STACK_BASE,
		.stack_size = STACK_SIZE,
		.tls_base   = TLS_BASE,
	};
	return sys_thrd_spawn(reprid, &param, 0);
}

void twz_thread_exit(void);
int twz_thread_wait(struct twzthread *th);

