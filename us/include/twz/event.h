#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

#include <twz/_types.h>

struct evhdr {
	_Atomic uint64_t point;
};

struct event {
	struct evhdr *hdr;
	uint64_t events;
	uint64_t result;
	uint64_t __resv[2];
	uint64_t flags;
};

void event_obj_init(twzobj *obj, struct evhdr *hdr);
void event_init(struct event *ev, struct evhdr *hdr, uint64_t events);
int event_wait(size_t count, struct event *ev, const struct timespec *);
int event_wake(struct evhdr *ev, uint64_t events, long wcount);
uint64_t event_clear(struct evhdr *hdr, uint64_t events);
void event_obj_init(twzobj *obj, struct evhdr *hdr);

static inline uint64_t event_poll(const struct evhdr *hdr, uint64_t events)
{
	return hdr->point & events;
}

#define EVENT_METAEXT_TAG 0x000000001122ee00eeee
