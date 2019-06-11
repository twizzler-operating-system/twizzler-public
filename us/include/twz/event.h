#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <time.h>

struct evhdr {
	_Atomic uint64_t point;
};

#define EV_TIMEOUT 1

struct event {
	struct evhdr *hdr;
	uint64_t events;
	uint64_t result;
	struct timespec timeout;
	uint64_t flags;
};

struct object;
void event_obj_init(struct object *obj, struct evhdr *hdr);
int event_init(struct event *ev, struct evhdr *hdr, uint64_t events, struct timespec *timeout);
int event_wait(size_t count, struct event *ev);
int event_wake(struct evhdr *ev, uint64_t events, long wcount);
void event_obj_init(struct object *obj, struct evhdr *hdr);

#define EVENT_METAEXT_TAG 0x000000001122ee00eeee
