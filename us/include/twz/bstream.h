#pragma once

#include <stdint.h>
#include <twz/event.h>
#include <twz/io.h>
#include <twz/mutex.h>

struct bstream_hdr {
	struct mutex rlock, wlock;
	uint32_t flags;
	_Atomic uint32_t head;
	_Atomic uint32_t tail;
	uint32_t nbits;
	struct evhdr ev;
	struct twzio_hdr io;
	unsigned char data[];
};

#define BSTREAM_METAEXT_TAG 0x00000000bbbbbbbb

#define BSTREAM_GATE_READ 1
#define BSTREAM_GATE_WRITE 2
