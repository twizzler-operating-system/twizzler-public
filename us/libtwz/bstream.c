#include <twz/bstream.h>
#include <twz/gate.h>
#include <twz/io.h>
#include <twz/obj.h>

int bstream_obj_init(struct object *obj, struct bstream_hdr *hdr)
{
}

ssize_t bstream_read(struct object *obj,
  struct bstream_hdr *hdr,
  void *ptr,
  size_t len,
  unsigned flags)
{
}

ssize_t bstream_write(struct object *obj,
  struct bstream_hdr *hdr,
  const void *ptr,
  size_t len,
  unsigned flags)
{
}

TWZ_GATE(bstream_read, 1);
TWZ_GATE(bstream_write, 2);
