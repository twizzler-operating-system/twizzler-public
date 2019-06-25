#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>
int __name_bootstrap(void);

void tmain(void *a)
{
	char *pg = a;
	objid_t id = 0;
	int r = twz_name_resolve(NULL, pg, NULL, 0, &id);
	if(r) {
		debug_printf("failed to resolve '%s'", pg);
		twz_thread_exit();
	}

	// twz_exec(id, (const char *const[]){ pg, NULL }, NULL);
	r = execv(pg, (char *[]){ pg, NULL });
	debug_printf("failed to exec '%s': %d", pg, r);
	twz_thread_exit();
}

struct object bs;
int main(int argc, char **argv)
{
	if(__name_bootstrap() == -1) {
		debug_printf("Failed to bootstrap namer\n");
		abort();
	}

	struct thread tthr;
	int r;
	if((r = twz_thread_spawn(
	      &tthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = "term.text" }))) {
		debug_printf("failed to spawn terminal");
		abort();
	}

	twz_thread_wait(1, (struct thread *[]){ &tthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

	int fd;
	if((fd = open("dev:dfl:keyboard", O_RDONLY)) != 0) {
		debug_printf("err opening stdin");
		abort();
	}
	if((fd = open("dev:dfl:screen", O_RDWR)) != 1) {
		debug_printf("err opening stdout");
		abort();
	}
	if((fd = open("dev:dfl:screen", O_RDWR)) != 2) {
		debug_printf("err opening stderr");
		abort();
	}

	printf("twzinit: starting\n");

	struct thread shthr;
	if((r = twz_thread_spawn(
	      &shthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = "shell.text" }))) {
		debug_printf("failed to spawn shell");
		abort();
	}

	printf("DONE\n");

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
