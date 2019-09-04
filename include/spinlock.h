#pragma once
#include <guard.h>
#include <stdatomic.h>

struct spinlock {
	_Atomic int data;
	bool fl;
};

#define DECLARE_SPINLOCK(name) struct spinlock name = { .data = 0 }

#define SPINLOCK_INIT                                                                              \
	(struct spinlock)                                                                              \
	{                                                                                              \
		.data = 0                                                                                  \
	}

bool __spinlock_acquire(struct spinlock *lock, const char *, int);
void __spinlock_release(struct spinlock *lock, bool, const char *, int);

#define spinlock_acquire(l) __spinlock_acquire(l, __FILE__, __LINE__)
#define spinlock_release(l, n) __spinlock_release(l, n, __FILE__, __LINE__)

#define spinlock_acquire_save(l) (l)->fl = __spinlock_acquire(l, __FILE__, __LINE__)
#define spinlock_release_restore(l) __spinlock_release(l, (l)->fl, __FILE__, __LINE__)
