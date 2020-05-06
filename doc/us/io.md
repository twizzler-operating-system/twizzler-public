Twizzler IO Objects (us/io.md)
===================

TWZIO_METAEXT_TAG

## twzio_hdr

``` {.c}
struct twzio_hdr {
	ssize_t (*read)(twzobj *, void *, size_t len, size_t off, unsigned flags);
	ssize_t (*write)(twzobj *, const void *, size_t len, size_t off, unsigned flags);
	int (*ioctl)(twzobj *, int request, long);
	int (*poll)(twzobj *, uint64_t type, struct event *event);
};
```

## Events

TWZIO_EVENT_READ
TWZIO_EVENT_WRITE
TWZIO_EVENT_IOCTL

## twzio_read, twzio_write

``` {.c}
ssize_t twzio_read(twzobj *obj, void *buf, size_t len, size_t off, unsigned flags);
```

``` {.c}
ssize_t twzio_write(twzobj *obj, const void *buf, size_t len, size_t off, unsigned flags);
```

``` {.c}
ssize_t twzio_ioctl(twzobj *obj, int req, ...);
```

``` {.c}
int twzio_poll(twzobj *, uint64_t, struct event *);
```

Valid flags include:

* `TWZIO_NONBLOCK`: Do not block if the operation cannot be performed immediately. If the function
  would have blocked, it returns `-EAGAIN`.

