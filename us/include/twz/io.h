#pragma once

#include <stddef.h>
#include <stdio.h>

#include <twz/_types.h>

struct twzio_hdr {
	ssize_t (*read)(twzobj *, void *, size_t len, size_t off, unsigned flags);
	ssize_t (*write)(twzobj *, const void *, size_t len, size_t off, unsigned flags);
	int (*ioctl)(twzobj *, int request, long);
};

#define TWZIO_METAEXT_TAG 0x0000000010101010

#define TWZIO_EVENT_READ 1
#define TWZIO_EVENT_WRITE 2
#define TWZIO_EVENT_IOCTL 3

ssize_t twzio_read(twzobj *obj, void *buf, size_t len, size_t off, unsigned flags);
ssize_t twzio_write(twzobj *obj, const void *buf, size_t len, size_t off, unsigned flags);
ssize_t twzio_ioctl(twzobj *obj, int req, ...);

#define TWZIO_NONBLOCK 1
