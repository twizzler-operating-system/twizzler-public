#include <stdatomic.h>
#include <stdbool.h>
#include <time.h>
#include <twz/event.h>
#include <twz/obj.h>
#include <twz/sys.h>

static int __event_wait(struct object *obj,
  struct evhdr *ev,
  uint64_t types,
  int flags,
  struct timespec *spec)
{
	_Atomic uint64_t *point = &ev->point;

	while(true) {
		uint64_t p = atomic_fetch_and(point, ~types);
		if(p & types)
			return __builtin_popcount(p & types);
		//	int r = sys_thread_sync(THREAD_SYNC_SLEEP, (long *)point, p, spec);
		//	if(r < 0)
		//		return r;
	}
}

static int __event_wake(struct object *obj, struct evhdr *ev, uint64_t types, long wcount)
{
	ev->point |= types;
	return sys_thread_sync(1,
	  (int[1]){ THREAD_SYNC_WAKE },
	  (long * [1]){ (long *)&ev->point },
	  (long[1]){ wcount },
	  NULL,
	  NULL);
}
