#pragma once
#include <lib/rb.h>

typedef uint64_t dur_nsec;

struct timer {
	dur_nsec time;
	size_t id;
	void (*fn)(void *);
	void *data;
	struct rbnode node;
	bool active;
};

void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data);
void timer_remove(struct timer *t);
uint64_t timer_check_timers(void);
