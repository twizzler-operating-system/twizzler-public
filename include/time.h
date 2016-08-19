#pragma once
#include <lib/linkedlist.h>

typedef uint64_t dur_nsec;

struct timer {
	dur_nsec time;
	void (*fn)(void *);
	void *data;
	struct linkedentry entry;
};

void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data);
