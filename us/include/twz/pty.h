#pragma once

#include <stdint.h>
#include <twz/event.h>
#include <twz/io.h>
#include <twz/mutex.h>

#include <twz/bstream.h>

struct pty_hdr {
	struct bstream_hdr *stoc;
	struct bstream_hdr *ctos;
};

#define PTY_METAEXT_TAG 0x0000000033333333

#define PTY_GATE_READ_SERVER 1
#define PTY_GATE_WRITE_SERVER 2
#define PTY_GATE_READ_CLIENT 3
#define PTY_GATE_WRITE_CLIENT 4

ssize_t pty_write_server(struct object *obj, const void *ptr, size_t len, unsigned flags);
ssize_t pty_read_server(struct object *obj, void *ptr, size_t len, unsigned flags);
ssize_t pty_write_client(struct object *obj, const void *ptr, size_t len, unsigned flags);
ssize_t pty_read_client(struct object *obj, void *ptr, size_t len, unsigned flags);
int pty_obj_init(struct object *obj, struct pty_hdr *hdr);
