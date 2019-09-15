#pragma once

#include <stdint.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <twz/bstream.h>
#include <twz/io.h>
#include <twz/mutex.h>

#define PTY_BUFFER_SZ 1024

struct pty_hdr {
	struct bstream_hdr *stoc;
	struct bstream_hdr *ctos;
	struct twzio_hdr io;
	struct termios termios;
	struct winsize wsz;
	struct mutex buffer_lock;
	size_t bufpos;
	char buffer[PTY_BUFFER_SZ];
};

struct pty_client_hdr {
	struct pty_hdr *server;
	struct twzio_hdr io;
};

#define PTY_CTRL_OBJ "/usr/bin/pty"

#define PTY_GATE_READ_SERVER 1
#define PTY_GATE_WRITE_SERVER 2
#define PTY_GATE_READ_CLIENT 3
#define PTY_GATE_WRITE_CLIENT 4
#define PTY_GATE_IOCTL_SERVER 5
#define PTY_GATE_IOCTL_CLIENT 6

ssize_t pty_write_server(struct object *obj, const void *ptr, size_t len, unsigned flags);
ssize_t pty_read_server(struct object *obj, void *ptr, size_t len, unsigned flags);
ssize_t pty_write_client(struct object *obj, const void *ptr, size_t len, unsigned flags);
ssize_t pty_read_client(struct object *obj, void *ptr, size_t len, unsigned flags);
int pty_obj_init_server(struct object *obj, struct pty_hdr *hdr);
int pty_obj_init_client(struct object *obj, struct pty_client_hdr *hdr, struct pty_hdr *);
