#include <twzio.h>
#include <twzviewcall.h>
#define TWZIO_METAHEADER (struct metaheader){.id = TWZIO_HEADER_ID, .len = sizeof(struct twzio_header) }

#include <debug.h>
struct twzio_header *twzio_init(struct object *obj, struct twzio_header *t)
{
	struct twzio_header *hdr = twz_object_addmeta(obj, TWZIO_METAHEADER);
	hdr->read = t->read;
	hdr->write = t->write;
	return hdr;
}

ssize_t twzio_read(struct object *obj, unsigned char *buf, size_t len, int flags)
{
	struct twzio_header *hdr = twz_object_findmeta(obj, TWZIO_HEADER_ID);
	if(!hdr || hdr->read == NULL) return -TE_NOTSUP;
	
	ssize_t (*_read)(struct object *, unsigned char *, size_t, unsigned) 
		= twz_ptr_lea(obj, hdr->read);
	ssize_t ret = _read(obj, buf, len, flags);
	return ret;
}

ssize_t twzio_write(struct object *obj, const unsigned char *buf, size_t len, int flags)
{
	struct twzio_header *hdr = twz_object_findmeta(obj, TWZIO_HEADER_ID);
	if(!hdr || hdr->write == NULL) return -TE_NOTSUP;
	
	ssize_t (*_write)(struct object *, const unsigned char *, size_t, unsigned) 
		= twz_ptr_lea(obj, hdr->write);
	debug_printf(":: write: %p\n", _write);
	return _write(obj, buf, len, flags);
}

