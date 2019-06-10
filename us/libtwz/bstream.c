#include <twz/bstream.h>
#include <twz/io.h>
#include <twz/obj.h>

int bstream_obj_init(struct object *obj, struct bstream_hdr *hdr)
{
}

__attribute__((externally_visible, used)) ssize_t bstream_read(struct object *obj,
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

#define TWZ_GATE(fn, g)                                                                            \
	__asm__(".section .gates, \"ax\", @progbits\n"                                                 \
	        ".global __twz_gate_" #fn "\n"                                                         \
	        ".type __twz_gate_" #fn " STT_FUNC\n"                                                  \
	        "__twz_gate_" #fn ": call " #fn "\n"                                                   \
	        ".previous");

TWZ_GATE(bstream_read, BSTREAM_GATE_READ);
