#pragma once
#include <lib/list.h>

typedef uint64_t dur_nsec;

struct timer {
	dur_nsec time;
	void (*fn)(void *);
	void *data;
	struct list entry;
};

void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data);
