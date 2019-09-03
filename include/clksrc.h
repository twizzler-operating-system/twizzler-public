#pragma once

#include <interrupt.h>

#define CLKSRC_MONOTONIC 1
#define CLKSRC_INTERRUPT 2
#define CLKSRC_ONESHOT 4
#define CLKSRC_PERIODIC 8

struct clksrc {
	uint64_t flags;
	uint64_t precision;
	uint64_t read_time;
	uint64_t period_ps;
	const char *name;
	void *priv;

	uint64_t (*read_counter)(struct clksrc *);
	void (*set_timer)(struct clksrc *, uint64_t ns, bool periodic);
	void (*set_active)(struct clksrc *, bool enable);

	struct list entry;
};

void clksrc_deregister(struct clksrc *cs);
void clksrc_register(struct clksrc *cs);
uint64_t clksrc_get_nanoseconds(void);
void clksrc_set_active(struct clksrc *cs, bool active);
bool clksrc_set_timer(struct clksrc *cs, uint64_t ns, bool periodic);
void clksrc_set_interrupt_countdown(uint64_t ns, bool periodic);
uint64_t clksrc_get_interrupt_countdown(void);
