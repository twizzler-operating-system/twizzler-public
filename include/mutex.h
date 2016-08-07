#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <thread.h>
#include <guard.h>

struct thread;
struct mutex {
	_Atomic int lock;
	struct blocklist wait;
#if CONFIG_DEBUG
	struct thread * _Atomic owner;
#endif
};

void mutex_acquire(struct mutex *);
void mutex_release(struct mutex *);
void mutex_create(struct mutex *);

#define mutex_guard(s) guard2(s,mutex_acquire,mutex_release)

