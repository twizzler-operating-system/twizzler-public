#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include <twz/_err.h>
#include <twz/debug.h>
#include <twz/event.h>
#include <twz/obj.h>
#include <twz/sys.h>

void event_obj_init(twzobj *obj, struct evhdr *hdr)
{
	(void)obj;
	hdr->point = 0;
}

static _Atomic uint64_t *__event_point(struct event *ev)
{
	return &ev->hdr->point;
}

void event_init(struct event *ev, struct evhdr *hdr, uint64_t events)
{
	ev->hdr = hdr;
	ev->events = events;
	ev->flags = 0;
}

uint64_t event_clear(struct evhdr *hdr, uint64_t events)
{
	uint64_t old = atomic_fetch_and(&hdr->point, ~events);
	return old & events;
}

int event_wait(size_t count, struct event *ev, const struct timespec *timeout)
{
	if(count > 4096) {
		return -EINVAL;
	}
	while(true) {
		struct sys_thread_sync_args args[count];
		size_t ready = 0;
		for(size_t i = 0; i < count; i++) {
			_Atomic uint64_t *point = __event_point(ev);
			args[i].arg = *point;
			args[i].addr = (uint64_t *)point;
			args[i].op = THREAD_SYNC_SLEEP;
			ev->result = args[i].arg & ev->events;
			if(ev->result) {
				ready++;
			}
		}
		if(ready > 0)
			return ready;

		struct timespec ts = timeout ? *timeout : (struct timespec){};
		int r = sys_thread_sync(count, args, timeout ? &ts : NULL);
		if(r < 0)
			return r;
	}
}

int event_wake(struct evhdr *ev, uint64_t events, long wcount)
{
	uint64_t old = atomic_fetch_or(&ev->point, events);
	if(((old & events) != events)) {
		struct sys_thread_sync_args args = {
			.op = THREAD_SYNC_WAKE,
			.addr = (uint64_t *)&ev->point,
			.arg = wcount,
		};
		return sys_thread_sync(1, &args, NULL);
	}
	return 0;
}
