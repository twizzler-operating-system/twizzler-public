#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
static int __name_bootstrap(void);
int main(int argc, char **argv)
{
	debug_printf("Testing!:: %s\n", getenv("BSNAME"));

	if(__name_bootstrap() == -1) {
		debug_printf("Failed to bootstrap namer\n");
		for(;;)
			;
	}
	objid_t id = 0;
	int r = twz_name_resolve(NULL, "test.text", NULL, 0, &id);
	debug_printf("NAME: " IDFMT " : %d\n", IDPR(id), r);

	struct object bs;
	id = 0;
	twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &id);
	twz_object_open(&bs, id, FE_READ | FE_WRITE);
	struct metainfo *mi = twz_object_meta(&bs);
	mi->milen = sizeof(*mi) + 128;

	r = bstream_obj_init(&bs, twz_obj_base(&bs), 8);

	struct bstream_hdr *hdr = twz_obj_base(&bs);
	debug_printf("%d:: %p %p\n", r, hdr->io.read, hdr->io.write);

	r = twzio_write(&bs, twz_obj_base(&bs), "hello\n", 6, 0, 0);
	debug_printf("write: %d\n", r);
	char buf[128] = { 0 };
	r = twzio_read(&bs, twz_obj_base(&bs), buf, 128, 0, 0);
	debug_printf("read: %d\n", r);
	debug_printf("read: %s\n", buf);

	for(;;)
		;

#if 0
	struct object obj;
	event_init(&obj, &e);

	int r;
	struct event res;
	while((r = event_wait(&obj, EV_READ, &res)) >= 0) {
		if(r == 0)
			continue;
		/* process events */
		if(res.events & EV_READ) {
		}
	}
#endif
}
