#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
static int __name_bootstrap(void);

void tmain(void *a)
{
	debug_printf("Hello from thread! %p\n", a);
	twz_thread_ready(NULL, THRD_SYNC_READY, 1234);
	for(;;)
		;
	struct object *b = a;
	debug_printf("WRITING\n");
	for(;;) {
		int r = bstream_write(b, twz_obj_base(b), "Hello!", 1, 0, 0);
		debug_printf("write: %d\n", r);
	}
	debug_printf("WRITING done\n");
	for(;;)
		;
}

struct object bs;
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

	struct thread t;

	id = 0;
	twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &id);
	twz_object_open(&bs, id, FE_READ | FE_WRITE);
	struct metainfo *mi = twz_object_meta(&bs);

	r = bstream_obj_init(&bs, twz_obj_base(&bs), 16);

	struct bstream_hdr *hdr = twz_obj_base(&bs);
	debug_printf("%d:: %p %p\n", r, hdr->io.read, hdr->io.write);

	r = twz_thread_spawn(&t, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &bs });
	debug_printf("spawn r = %d\n", r);

	struct thread *w = &t;
	uint64_t info;
	r = twz_thread_wait(1, &w, (int[]){ THRD_SYNC_READY }, NULL, &info);
	debug_printf("WAIT RET %d: %ld\n", r, info);

	for(;;)
		;

	// r = twzio_write(&bs, twz_obj_base(&bs), "hello\n", 6, 0, 0);
	char buf[1 << 15] = { 0 };
	for(;;) {
		r = twzio_read(&bs, twz_obj_base(&bs), buf, 1 << 15, 0, 0);
		debug_printf("read: %d\n", r);
		// debug_printf("read: %s\n", buf);
	}

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
