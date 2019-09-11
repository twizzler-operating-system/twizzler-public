#include <string.h>
#include <twz/event.h>
#include <twz/gate.h>
#include <twz/io.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/pty.h>

#include <twz/debug.h>

#define PTY_NBITS 11

ssize_t pty_read_server(struct object *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
}

ssize_t pty_write_server(struct object *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
}

ssize_t pty_read_client(struct object *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
}

ssize_t pty_write_client(struct object *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
}

int pty_obj_init(struct object *obj, struct pty_hdr *hdr)
{
	hdr->stoc = (struct bstream_hdr *)twz_ptr_local(hdr);
	hdr->ctos = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + bstream_hdr_size(PTY_NBITS));

	bstream_obj_init(obj, hdr->stoc, PTY_NBITS);
	bstream_obj_init(obj, hdr->stoc, PTY_NBITS);
	return 0;
}
