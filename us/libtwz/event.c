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
		struct sys_thread_sync_args args[count];
		int ops[count];
		long *points[count];
		long arg[count];
		struct timespec *spec[count];
		size_t ready = 0;
		for(size_t i = 0; i < count; i++) {
			_Atomic uint64_t *point = __event_point(ev);
			args[i].arg = *point;
			args[i].addr = (uint64_t *)point;
			args[i].op = THREAD_SYNC_SLEEP;
			if(ev->flags & EV_TIMEOUT) {
				args[i].spec = &ev->timeout;
				args[i].flags = THREAD_SYNC_TIMEOUT;
			} else {
				args[i].flags = 0;
			}
			ev->result = args[i].arg & ev->events;
			if(ev->result) {
				ready++;
			}
		}
		if(ready > 0)
			return ready;

		int r = sys_thread_sync(count, args);
		if(r < 0)
			return r;
	}
}

int event_wake(struct evhdr *ev, uint64_t events, long wcount)
{
	uint64_t old = atomic_fetch_or(&ev->point, events);
	if((old & events) != events) {
		struct sys_thread_sync_args args = {
			.op = THREAD_SYNC_WAKE,
			.addr = (uint64_t *)&ev->point,
			.arg = wcount,
		};
		return sys_thread_sync(1, &args);
	}
	return 0;
}
