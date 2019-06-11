#include <string.h>
#include <twz/bstream.h>
#include <twz/event.h>
#include <twz/gate.h>
#include <twz/io.h>
#include <twz/obj.h>

#include <twz/debug.h>

static size_t free_space(size_t head, size_t tail, size_t length)
{
	return (tail > head) ? tail - head : length - head + tail;
}

ssize_t bstream_read(struct object *obj,
  struct bstream_hdr *hdr,
  void *ptr,
  size_t len,
  unsigned flags)
{
	mutex_acquire(&hdr->rlock);

	size_t count = 0;
	unsigned char *data = ptr;
	for(size_t i = 0; i < len; i++) {
		if(hdr->head == hdr->tail)
			break;
		count++;
		data[i] = hdr->data[hdr->tail];
		hdr->tail = (hdr->tail + 1) % (1 << hdr->nbits);
	}

	mutex_release(&hdr->rlock);
	return count;
}

ssize_t bstream_write(struct object *obj,
  struct bstream_hdr *hdr,
  const void *ptr,
  size_t len,
  unsigned flags)
{
	debug_printf("W\n");
	mutex_acquire(&hdr->wlock);

	size_t count = 0;
	const unsigned char *data = ptr;
	for(size_t i = 0; i < len; i++) {
		debug_printf(".");
		if(free_space(hdr->head, hdr->tail, 1 << hdr->nbits) <= 1)
			break;
		count++;
		hdr->data[hdr->head] = data[i];
		hdr->head = (hdr->head + 1) % (1 << hdr->nbits);
	}

	mutex_release(&hdr->wlock);
	return count;
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

	objid_t id;
	r = twz_name_resolve(NULL, "libtwz.so.text", NULL, 0, &id);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, BSTREAM_GATE_READ), FE_READ | FE_EXEC, &hdr->io.read);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, BSTREAM_GATE_WRITE), FE_READ | FE_EXEC, &hdr->io.write);
	if(r)
		return r;
}

TWZ_GATE(bstream_read, BSTREAM_GATE_READ);
TWZ_GATE(bstream_write, BSTREAM_GATE_WRITE);
