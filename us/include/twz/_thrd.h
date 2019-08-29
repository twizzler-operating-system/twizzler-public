#pragma once

#include <twz/_fault.h>
#include <twz/_kso.h>
#include <twz/_objid.h>
#include <twz/_view.h>

#define THRD_SYNCPOINTS 128

#define THRD_SYNC_SPAWNED 0
#define THRD_SYNC_READY 1
#define THRD_SYNC_EXIT 2

#define TWZ_THRD_MAX_SCS 32

struct twzthread_repr {
	struct kso_hdr hdr;
	objid_t reprid;
	_Atomic uint64_t syncs[THRD_SYNCPOINTS];
	uint64_t syncinfos[THRD_SYNCPOINTS];
	struct faultinfo faults[NUM_FAULTS];
	struct kso_attachment attached[TWZ_THRD_MAX_SCS];
	struct viewentry fixed_points[];
};
