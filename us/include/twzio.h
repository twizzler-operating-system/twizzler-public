#pragma once

#include <twzobj.h>
#define TWZIO_HEADER_ID 3

struct twzio_header {
	struct metaheader _header;
	ssize_t (*read)(struct object *, unsigned char *, size_t len, unsigned flags);
	ssize_t (*write)(struct object *, const unsigned char *, size_t len, unsigned flags);
};

#define TWZIO_NONBLOCK 1
#define TWZIO_LINEBUF  2

ssize_t twzio_write(struct object *obj, const unsigned char *buf, size_t len, int flags);
ssize_t twzio_read(struct object *obj, unsigned char *buf, size_t len, int flags);
struct twzio_header *twzio_init(struct object *obj, struct twzio_header *t);

