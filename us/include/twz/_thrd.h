#pragma once

#include <twz/_fault.h>
#include <twz/_objid.h>
#include <twz/_view.h>

#define THRD_SYNCPOINTS 128

#define THRD_SYNC_SPAWNED 0
#define THRD_SYNC_READY 1
#define THRD_SYNC_EXIT 2

struct twzthread_repr {
	objid_t reprid;
	uint64_t syncs[THRD_SYNCPOINTS];
	struct faultinfo faults[NUM_FAULTS];
	struct viewentry fixed_points[];
};

struct thrd_spawn_args {
	objid_t target_view;
	void (*start_func)(void *); /* thread entry function. */
	void *arg;                  /* argument for entry function. */
	char *stack_base;           /* stack base address. */
	size_t stack_size;
	char *tls_base; /* tls base address. */
};
