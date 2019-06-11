#pragma once

#include <stddef.h>
#include <stdio.h>

struct object;
struct twzio_hdr {
	ssize_t (*read)(struct object *, void *, size_t len, size_t off, unsigned flags);
	ssize_t (*write)(struct object *, const void *, size_t len, size_t off, unsigned flags);
};

#define TWZIO_METAEXT_TAG 0x0000000010101010

#define TWZIO_EVENT_READ 1
#define TWZIO_EVENT_WRITE 2
