Twizzler Thread API
===================

## twz_thread_spawn

``` {.c}
int twz_thread_spawn(struct thread *thrd, struct thrd_spawn_args *args);
```

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

``` {.c}
ssize_t twz_thread_wait(struct thrd_wait_args *args, size_t count);
```

``` {.c}
struct thrd_wait_args {
	struct thread *thread;
	int syncpoint;
	long event;
	uint64_t info;
};
```

## twz_thread_ready

``` {.c}
int twz_thread_ready(struct thread *thread, int sp, uint64_t info);
```

## twz_thread_repr_base

``` {.c}
void *twz_thread_repr_base(void);
```

## twz_stdstack

The `twz/thread.h` header provides an object, `twz_stdstack`, which is a `struct object` that refers
to the standard stack object.


