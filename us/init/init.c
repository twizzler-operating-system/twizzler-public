#include <stdlib.h>
#include <twz/debug.h>
#include <twz/name.h>
static int __name_boostrap(void);
int main(int argc, char **argv)
{
	debug_printf("Testing!:: %s\n", getenv("BSNAME"));

	if(__name_boostrap() == -1) {
		debug_printf("Failed to bootstrap namer\n");
		for(;;)
			;
	}
	objid_t id = 0;
	int r = twz_name_resolve(NULL, "test.text", NULL, 0, &id);
	debug_printf("NAME: " IDFMT " : %d\n", IDPR(id), r);
	bstream_read();
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
