#pragma once
#include <stdatomic.h>
#include <guard.h>

struct spinlock {
	_Atomic int data;
	bool fl;
};

#define DECLARE_SPINLOCK(name) struct spinlock name = { .data = 0 }

#define SPINLOCK_INIT (struct spinlock) { .data = 0 }

bool spinlock_acquire(struct spinlock *lock);
void spinlock_release(struct spinlock *lock, bool);

#define spinlock_acquire_save(l) (l)->fl = spinlock_acquire(l)
#define spinlock_release_restore(l) spinlock_release(l, (l)->fl)

