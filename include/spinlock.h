#pragma once
#include <stdatomic.h>
#include <guard.h>

struct spinlock {
	_Atomic int data;
};

#define SPINLOCK_INIT (struct spinlock) { .data = 0 }

bool spinlock_acquire(struct spinlock *lock);
void spinlock_release(struct spinlock *lock, bool);

