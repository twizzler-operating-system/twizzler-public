#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <twz/_err.h>
#include <twz/debug.h>
#include <twz/event.h>
#include <twz/obj.h>
#include <twz/sys.h>

/* TODO: update kernel API to allow rechecking value instead of returning whenever woken up? or just
 * update api so that we can sleep on a bitwise level (sleep if this bit is set), etc */

void event_obj_init(struct object *obj, struct evhdr *hdr)
{
	hdr->point = 0;
}

static _Atomic uint64_t *__event_point(struct event *ev)
{
	return &ev->hdr->point;
}

int event_init(struct event *ev, struct evhdr *hdr, uint64_t events, struct timespec *timeout)
{
	ev->hdr = hdr;
	ev->events = events;
	ev->flags = 0;
	if(timeout) {
		ev->timeout = *timeout;
		ev->flags |= EV_TIMEOUT;
	}
	return 0;
}

uint64_t event_clear(struct evhdr *hdr, uint64_t events)
{
	uint64_t old = atomic_fetch_and(&hdr->point, ~events);
	return old & events;
}

int event_wait(size_t count, struct event *ev)
{
	if(count > 4096)
		return -EINVAL; // TODO
	while(true) {
		int ops[count];
		long *points[count];
		long arg[count];
		struct timespec *spec[count];
		size_t ready = 0;
		for(size_t i = 0; i < count; i++) {
			_Atomic uint64_t *point = __event_point(ev);
			arg[i] = *point;
			points[i] = (long *)point;
			ops[i] = THREAD_SYNC_SLEEP;
			spec[i] = ev->flags & EV_TIMEOUT ? &ev->timeout : NULL;
			debug_printf("== %lx %lx\n", *point, ev->events);
			ev->result = arg[i] & ev->events;
			if(ev->result) {
				ready++;
			}
		}
		if(ready > 0)
			return ready;

		int r = sys_thread_sync(count, ops, points, arg, NULL, spec);
		if(r < 0)
			return r;
	}
}

int event_wake(struct evhdr *ev, uint64_t events, long wcount)
{
	uint64_t old = atomic_fetch_or(&ev->point, events);
	if((old & events) != events) {
		return sys_thread_sync(1,
		  (int[1]){ THREAD_SYNC_WAKE },
		  (long * [1]){ (long *)&ev->point },
		  (long[1]){ wcount },
		  NULL,
		  NULL);
	}
	return 0;
}
