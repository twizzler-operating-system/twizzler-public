#pragma once
#include <stdatomic.h>

struct spinlock {
	_Atomic int data;
};

#define SPINLOCK_INIT (struct spinlock) { .data = 0 }

void spinlock_acquire(struct spinlock *lock);
void spinlock_release(struct spinlock *lock);

