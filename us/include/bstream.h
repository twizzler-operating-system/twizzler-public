#pragma once

#include <stdint.h>
#include <stddef.h>
#include <twzobj.h>
#include <notify.h>
#include <mutex.h>
#include <limits.h>

#include <twzio.h>

#include <stdatomic.h>
struct bstream_header {
	struct metaheader _header;
	struct mutex readlock, writelock;
	uint32_t flags;
	uint32_t head;
	uint32_t tail;
	uint32_t nbits;
	uint64_t rwait;
	uint64_t wwait;
	int rwid, wwid;
};

#define BSTREAM_HEADER_ID 1

int bstream_init(struct object *obj, int nbits);
int bstream_getb(struct object *obj, unsigned fl);
int bstream_putb(struct object *obj, unsigned char c, unsigned fl);
void bstream_notify_prepare(struct object *obj);
ssize_t bstream_read(struct object *obj, unsigned char *buf,
		size_t len, unsigned fl);
ssize_t bstream_write(struct object *obj, const unsigned char *buf,
		size_t len, unsigned fl);


