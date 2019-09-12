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
	return bstream_hdr_read(obj, twz_ptr_lea(obj, hdr->ctos), ptr, len, flags);
}

ssize_t pty_write_server(struct object *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
	return bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->stoc), ptr, len, flags);
}

ssize_t pty_read_client(struct object *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_client_hdr *hdr = twz_obj_base(obj);
	struct pty_hdr *sh = twz_ptr_lea(obj, hdr->server);
	struct object sh_obj = TWZ_OBJECT_FROM_PTR(sh);
	return bstream_hdr_read(&sh_obj, twz_ptr_lea(&sh_obj, sh->stoc), ptr, len, flags);
}

ssize_t pty_write_client(struct object *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_client_hdr *hdr = twz_obj_base(obj);
	struct pty_hdr *sh = twz_ptr_lea(obj, hdr->server);
	struct object sh_obj = TWZ_OBJECT_FROM_PTR(sh);
	return bstream_hdr_write(&sh_obj, twz_ptr_lea(&sh_obj, sh->ctos), ptr, len, flags);
}

int pty_obj_init_server(struct object *obj, struct pty_hdr *hdr)
{
	hdr->stoc = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + sizeof(struct pty_hdr));
	hdr->ctos = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + sizeof(struct pty_hdr)
	                                   + bstream_hdr_size(PTY_NBITS));

	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io)))
		return r;
	bstream_obj_init(obj, twz_ptr_lea(obj, hdr->stoc), PTY_NBITS);
	bstream_obj_init(obj, twz_ptr_lea(obj, hdr->ctos), PTY_NBITS);

	objid_t id;
	r = twz_name_resolve(NULL, "pty.text", NULL, 0, &id);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_READ_SERVER), FE_READ | FE_EXEC, &hdr->io.read);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_WRITE_SERVER), FE_READ | FE_EXEC, &hdr->io.write);
	if(r)
		return r;

	return 0;
}

int pty_obj_init_client(struct object *obj, struct pty_client_hdr *hdr, struct pty_hdr *sh)
{
	twz_ptr_store(obj, sh, FE_READ | FE_WRITE, &hdr->server);

	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io)))
		return r;
	objid_t id;
	r = twz_name_resolve(NULL, "pty.text", NULL, 0, &id);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_READ_CLIENT), FE_READ | FE_EXEC, &hdr->io.read);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_WRITE_CLIENT), FE_READ | FE_EXEC, &hdr->io.write);
	if(r)
		return r;

	return 0;
}
