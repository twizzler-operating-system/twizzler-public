#pragma once

#include <stddef.h>
#include <stdio.h>

struct object;
struct twzio_hdr {
	ssize_t (*read)(struct object *, void *, size_t len, size_t off, unsigned flags);
	ssize_t (*write)(struct object *, const void *, size_t len, size_t off, unsigned flags);
	int (*ioctl)(struct object *, int request, long);
};

#define TWZIO_METAEXT_TAG 0x0000000010101010

#define TWZIO_EVENT_READ 1
#define TWZIO_EVENT_WRITE 2
#define TWZIO_EVENT_IOCTL 3

ssize_t twzio_read(struct object *obj, void *buf, size_t len, size_t off, unsigned flags);
ssize_t twzio_write(struct object *obj, const void *buf, size_t len, size_t off, unsigned flags);
ssize_t twzio_ioctl(struct object *obj, int req, long arg);

#define TWZIO_NONBLOCK 1
