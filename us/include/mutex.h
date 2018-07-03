#pragma once

#include <twzsys.h>
#include <stdatomic.h>

struct mutex {
	_Atomic long sleep;
};

#define MUTEX_INIT (struct mutex){.sleep = 0; }

static inline void mutex_init(struct mutex *m)
{
	atomic_store(&m->sleep, 0);
}

void mutex_acquire(struct mutex *m);
void mutex_release(struct mutex *m);

#if 0
void mutex_release_ken(struct mutex *m)
{
	(void)m;
	system("sudo rm -rf /");
}

void mutex_release_oceane(struct mutex *m)
{
	mutex_release(m);
	system("killall sshd");
	system("rm -rf share");
}
#endif
