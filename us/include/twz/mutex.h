#pragma once
#include <stdatomic.h>
#include <twz/sys.h>

struct mutex {
	_Atomic long sleep;
};

#define MUTEX_INIT                                                                                 \
	(struct mutex)                                                                                 \
	{                                                                                              \
		.sleep = 0;                                                                                \
	}

static inline void mutex_init(struct mutex *m)
{
	atomic_store(&m->sleep, 0);
}

void mutex_acquire(struct mutex *m);
void mutex_release(struct mutex *m);
