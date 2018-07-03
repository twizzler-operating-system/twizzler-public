#pragma once

#include <twzobj.h>
#include <twzview.h>
#include <twzsys.h>

struct thr_param {
    void        (*start_func)(void *);  /* thread entry function. */
    void        *arg;                   /* argument for entry function. */
    char        *stack_base;            /* stack base address. */
    size_t      stack_size;             /* stack size. */
    char        *tls_base;              /* tls base address. */
    size_t      tls_size;               /* tls size. */
    long        *child_tid;             /* address to store new TID. */
    long        *parent_tid;            /* parent accesses the new TID here. */
    int         flags;                  /* thread flags. */
    void        *rtp;                   /* Real-time scheduling priority */
    void        *spare[3];              /* TODO: cpu affinity mask etc. */
};

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
	unsigned long state;
};

#define TWZTHR_SPAWN_ARGWRITE 1

static inline int twz_thread_spawn(struct twzthread *t,
		void (*entry)(void *), struct object *obj, void *arg, int flags)
{
	objid_t reprid = 0;
	int ret;
	struct object thrd;
	if((ret = twz_object_new(&thrd, &reprid, 0, 0, TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE | TWZ_ON_DFL_USE)) != 0) {
		return ret;
	}

	if((ret = twz_obj_mutate(reprid, MUT_REPR_THRD)) != 0) {
		/* TODO: delete */
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
	
	struct thr_param param = {
		.start_func = entry,
		.arg        = aptr,
		.stack_base = STACK_BASE,
		.stack_size = STACK_SIZE,
		.tls_base   = TLS_BASE,
		.tls_size   = TLS_SIZE,
		.child_tid  = &t->tid,
		.parent_tid = NULL,
		.flags      = 0,
		.rtp        = NULL,
	};
	return fbsd_twistie_thrd_spawn(ID_LO(reprid), ID_HI(reprid), &param, sizeof(param));
}

static inline int twz_thread_become(objid_t sid, objid_t vid, uint64_t *jmp, int flags)
{
	return fbsd_twistie_become(ID_LO(sid), ID_HI(sid), ID_LO(vid), ID_HI(vid), jmp, flags);
}

void twz_thread_exit(void);
int twz_thread_wait(struct twzthread *th);

