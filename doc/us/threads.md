Twizzler Thread API
===================

...threads have thread control objects... TODO

## twz_thread_spawn

``` {.c}
int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args);
```

Spawn a new thread, initializing the handle `thrd` for future use interacting with the thread.
The new thread will be initialized according to `args`:

``` {.c}
struct thrd_spawn_args {
	objid_t target_view;
	void (*start_func)(void *); /* thread entry function. */
	void *arg;                  /* argument for entry function. */
	char *stack_base;           /* stack base address. */
	size_t stack_size;
	char *tls_base; /* tls base address. */
};
```

If `target_view` is zero, the thread will be given the same view as the current thread. The thread
will be started in `start_func` with argument `arg`.

Optionally, the thread can be given a stack base address and a stack size. If `stack_base` is set to
NULL then the thread will be given a new stack object automatically.

### Return Value

### Errors
This function can return any error returned by `sys_thrd_spawn` or `twz_object_create`.

## twz_thread_wait

``` {.c}
ssize_t twz_thread_wait(struct thrd_wait_args *args, size_t count);
```

Wait for thread(s) on (a) particular sync point(s). All thread control objects have a collection of
sync points. A sync point with value 0 indicates "not ready", whereas a sync point with a non-zero
value indicates ready (and the value can be interpreted as some kind of info).

The function takes an array of `struct thrd_wait_args`, each of which defines a sync point to wait
on for a thread. The function waits for any one of the specified sync points to be ready before
returning. If a sync point is ready, the `info` field of the associated `struct thrd_wait_args` is
filled out with the value of the sync point. The `event` field is set to a non-zero value as well.

``` {.c}
struct thrd_wait_args {
	struct thread *thread;
	int syncpoint;
	long event;
	uint64_t info;
};
```

The `struct thrd_wait_args`'s field `thread` specifies which thread to wait for, and the
`syncpoint` field specifies which sync point to wait for. Twizzler defines the following standard
sync points:

* `THRD_SYNC_SPAWNED`: Set when the thread is spawned. Value is unspecified.
* `THRD_SYNC_READY`: Set when the thread is "ready". Value is unspecified. What ready means depends
  on the thread and the application. Often set when the thread is done with initialization.
* `THRD_SYNC_EXIT`: Set when the thread exits. The value is the exit code.

### Return Value
Returns a count of how many sync points are ready on success, error code on error.

### Errors
This function can return any error returned by `sys_thrd_sync`.

## twz_thread_ready

``` {.c}
int twz_thread_ready(struct thread *thread, int sp, uint64_t info);
```

Mark a sync point `sp` for thread `thread` as ready, using `info` as the value for the sync point.
The `info` argument must be non-zero for threads to determine that `sp` is ready.
If `thread` is NULL, use the current thread. Must have write access to
the specified thread control object.

### Return Value
Returns 0 on success, error code on error.

### Errors

* `-EINVAL`: Invalid argument

## twz_thread_repr_base

``` {.c}
void *twz_thread_repr_base(void);
```

Determine a d-ptr to the base of the current thread's control object.

### Return Value
Returns a pointer to the base of the current thread control object. This function always succeeds.

## twz_stdstack

The `twz/thread.h` header provides an object, `twz_stdstack`, which is a `twzobj *` that refers
to the standard stack object.


