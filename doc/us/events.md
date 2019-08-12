Events API (us/events.md)
==========

The event waiting and waking system functions in two parts:

* The waiter constructs a set of events upon which it waits, and then calls the wait function.
* The waker, when appropriate, wakes up all threads waiting on a particular event.

An object which implements the events API through structured objects can be found with the `EVENT_METAEXT_TAG`.

An events-enabled object will have a `struct evhdr` inside it which is used for waiting on events in
that object. Every `struct evhdr` provides 64 unique events that can waited on, each a single bit in
a 64-bit value. From here, the term "event" means a particular bit in a particular `evhdr`. 
If the bit is set, the event is "available", meaning that it has occurred, and
something is ready. It will stay set until it is explicitly cleared (by `event_clear`). A thread
which calls `event_wait` on an event that is not ready will (probably) sleep, whereas if it calls
`event_wait` on an event which is active (bit set), the thread will not sleep. When waking a thread,
the waker calls `event_wake`, which sets the specified bits (indicating the specified events are
ready) and then wakes up waiting threads.

## event_obj_init
``` {.c}
void event_obj_init(struct object *obj, struct evhdr *hdr);
```

Initialize a `struct evhdr` within object `obj`. Must be called before any function that operates on
the `hdr`.

## event_init
``` {.c}
int event_init(struct event *ev, struct evhdr *hdr, uint64_t events, struct timespec *timeout);
```

Prepare a `struct event` (`ev`) for waiting. The `ev` argument will be filled out with the provided
information and will be ready to be passwd `event_wake` function. The `events` argument is a
bitfield of events that the caller wishes to wait on (eg. if `events` is 3, then we are waiting on
event 1 and/or event 2). If the timeout field is NULL, then no timeout will be specified.

### Return Value
Returns 0 on success, error code on error.
### Errors
* `-EINVAL`: Invalid argument

## event_wait
``` {.c}
int event_wait(size_t count, struct event *ev);
```
Wait on a set of events. The `count` argument describes the length of the array `ev`, which must
contain a valid `struct event` (initialized via `event_init`) for each entry. The function will then
wait for any specified event to occur.

The `struct event` has a result field (type `uint64_t`) which gets a copy of the available events
for that object when the function returns.

This function can return spuriously.

### Return Value
Returns the number of ready events on success, error code on error (negative).

### Errors
This function can return any error specified by the `sys_thrd_sync` system call.

## event_wake
``` {.c}
int event_wake(struct evhdr *ev, uint64_t events, long wcount);
```

Mark the events specified in `events` as occurred in `ev`, and `wcount` threads waiting for an event
on this object. If wcount is -1, wakeup all threads. Note that there is no guarantee that the woken threads are _actually_ waiting for
the events specified in `events`, just that they are waiting for _an_ event on this object. It is
recommended to pass -1 for wcount unless you know what you're doing.

### Return Value
Returns 0 on success, error code on error.

### Errors
This function may return any error returned by `sys_thrd_sync`.

## event_clear
``` {.c}
uint64_t event_clear(struct evhdr *hdr, uint64_t events);
```

Clear the events specified by `events` from `hdr`, returning any of those events if they are active.
This function does not attempt to wake any threads, nor does it wait.

### Return Value
Returns a bitwise-and of the pre-cleared events and the `events` argument, effectively returning
which events were cleared. This function does not fail.

