#include <string.h>
#include <twz/bstream.h>
#include <twz/event.h>
#include <twz/gate.h>
#include <twz/io.h>
#include <twz/obj.h>

#include <twz/debug.h>
ssize_t bstream_read(struct object *obj,
  struct bstream_hdr *hdr,
  void *ptr,
  size_t len,
  unsigned flags)
{
	debug_printf("HELLO!\n");
}

ssize_t bstream_write(struct object *obj,
  struct bstream_hdr *hdr,
  const void *ptr,
  size_t len,
  unsigned flags)
{
}

int bstream_obj_init(struct object *obj, struct bstream_hdr *hdr, uint32_t nbits)
{
	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io)))
		return r;
	if((r = twz_object_addext(obj, EVENT_METAEXT_TAG, &hdr->ev)))
		return r;
	memset(hdr, 0, sizeof(*hdr));
	mutex_init(&hdr->rlock);
	mutex_init(&hdr->wlock);
	hdr->nbits = nbits;
	event_obj_init(obj, &hdr->ev);
	r =
	  twz_ptr_store(obj, TWZ_GATE_CALL(NULL, BSTREAM_GATE_READ), FE_READ | FE_EXEC, &hdr->io.read);
	if(r)
		return r;
	r = twz_ptr_store(
	  obj, TWZ_GATE_CALL(NULL, BSTREAM_GATE_WRITE), FE_READ | FE_EXEC, &hdr->io.write);
	if(r)
		return r;
}

TWZ_GATE(bstream_read, BSTREAM_GATE_READ);
TWZ_GATE(bstream_write, BSTREAM_GATE_WRITE);
