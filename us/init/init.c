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

	objid_t id = 0;
	int r = twz_name_resolve(NULL, "term.text", NULL, 0, &id);
	if(r) {
		debug_printf("FAILED to resolve term");
		twz_thread_exit();
	}

	debug_printf("EXECUTE " IDFMT, IDPR(id));
	twz_exec(id, (const char *const[]){ "term.text", "arg1", "arg2", NULL }, NULL);
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
	r = twz_thread_spawn(&t, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &bs });
	debug_printf("spawn r = %d\n", r);

	struct thread *w = &t;
	uint64_t info;
	r = twz_thread_wait(1, &w, (int[]){ THRD_SYNC_READY }, NULL, &info);
	debug_printf("WAIT RET %d: %ld\n", r, info);

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
