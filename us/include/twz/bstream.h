#pragma once

#include <twz/__twz.h>

#include <stdint.h>
#include <twz/event.h>
#include <twz/io.h>
#include <twz/mutex.h>

struct bstream_hdr {
	// struct mutex rlock, wlock;
	struct mutex lock;
	uint32_t flags;
	_Atomic uint32_t head;
	_Atomic uint32_t tail;
	uint32_t nbits;
	struct evhdr ev;
	struct twzio_hdr io;
	char coname1[64];
	char coname2[64];
	char coname3[64];
	char coname4[64];
	unsigned char data[];
};

static inline size_t bstream_hdr_size(uint32_t nbits)
{
	return sizeof(struct bstream_hdr) + (1ul << nbits);
}

#define BSTREAM_CTRL_OBJ "/usr/bin/bstream"

#define BSTREAM_METAEXT_TAG 0x00000000bbbbbbbb

#define BSTREAM_GATE_READ 1
#define BSTREAM_GATE_WRITE 2
#define BSTREAM_GATE_POLL 3

ssize_t bstream_write(twzobj *obj, const void *ptr, size_t len, unsigned flags);
ssize_t bstream_read(twzobj *obj, void *ptr, size_t len, unsigned flags);
ssize_t bstream_hdr_write(twzobj *obj,
  struct bstream_hdr *,
  const void *ptr,
  size_t len,
  unsigned flags);
ssize_t bstream_hdr_read(twzobj *obj, struct bstream_hdr *, void *ptr, size_t len, unsigned flags);
int bstream_poll(twzobj *obj, uint64_t type, struct event *event);
int bstream_hdr_poll(twzobj *obj, struct bstream_hdr *hdr, uint64_t type, struct event *event);

__must_check int bstream_obj_init(twzobj *obj, struct bstream_hdr *hdr, uint32_t nbits);
