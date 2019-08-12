Events API (us/events.md)
==========

EVENT_METAEXT_TAG

struct evhdr

## event_obj_init
``` {.c}
void event_obj_init(struct object *obj, struct evhdr *hdr);
```

## event_init
``` {.c}
int event_init(struct event *ev, struct evhdr *hdr, uint64_t events, struct timespec *timeout);
```

## event_wake
``` {.c}
int event_wake(struct evhdr *ev, uint64_t events, long wcount);
```

## event_clear
``` {.c}
uint64_t event_clear(struct evhdr *hdr, uint64_t events);
```

## event_wait
``` {.c}
int event_wait(size_t count, struct event *ev);
```

EV_TIMEOUT

``` {.c}
struct event {
	struct evhdr *hdr;
	uint64_t events;
	uint64_t result;
	struct timespec timeout;
	uint64_t flags;
};
```

