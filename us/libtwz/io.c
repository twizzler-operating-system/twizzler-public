#include <twz/_err.h>
#include <twz/io.h>
#include <twz/obj.h>

#include <twz/debug.h>
ssize_t twzio_read(struct object *obj, void *buf, size_t len, size_t off, unsigned flags)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr || !hdr->read)
		return -ENOTSUP;

	void *_fn = twz_ptr_lea(obj, hdr->read);
	if(!_fn)
		return -EGENERIC;
	ssize_t (*fn)(struct object *, void *, size_t, size_t, unsigned) = _fn;
	return fn(obj, buf, len, off, flags);
}

ssize_t twzio_write(struct object *obj, const void *buf, size_t len, size_t off, unsigned flags)
{
	struct twzio_hdr *hdr = twz_object_getext(obj, TWZIO_METAEXT_TAG);
	if(!hdr || !hdr->write)
		return -ENOTSUP;

	void *_fn = twz_ptr_lea(obj, hdr->write);
	if(!_fn)
		return -EGENERIC;
	ssize_t (*fn)(struct object *, const void *, size_t, size_t, unsigned) = _fn;
	return fn(obj, buf, len, off, flags);
}
