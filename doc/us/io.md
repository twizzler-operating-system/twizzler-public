Twizzler IO Objects (us/io.md)
===================

TWZIO_METAEXT_TAG

## twzio_hdr

``` {.c}
struct twzio_hdr {
	ssize_t (*read)(struct object *, void *, size_t len, size_t off, unsigned flags);
	ssize_t (*write)(struct object *, const void *, size_t len, size_t off, unsigned flags);
};
```

## Events

TWZIO_EVENT_READ
TWZIO_EVENT_WRITE

## twzio_read, twzio_write

``` {.c}
ssize_t twzio_read(struct object *obj, void *buf, size_t len, size_t off, unsigned flags);
```

``` {.c}
ssize_t twzio_write(struct object *obj, const void *buf, size_t len, size_t off, unsigned flags);
```

Valid flags include:

* `TWZIO_NONBLOCK`: Do not block if the operation cannot be performed immediately. If the function
  would have blocked, it returns `-EAGAIN`.

