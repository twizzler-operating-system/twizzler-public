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
	uint64_t syncinfos[THRD_SYNCPOINTS];
	struct faultinfo faults[NUM_FAULTS];
	struct viewentry fixed_points[];
};
