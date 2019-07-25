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

struct service_info {
	objid_t sctx;
	const char *name;
};

void tmain(void *a)
{
	// struct service_info *info = twz_ptr_lea(twz_stdstack, a);
	char *pg = twz_ptr_lea(twz_stdstack, a);
	int r;

	if(!strcmp(pg, "login.text")) {
		objid_t si;
		int r = twz_name_resolve(NULL, "login.sctx", NULL, 0, &si);
		if(r) {
			debug_printf("failed to resolve 'login.sctx'");
			twz_thread_exit();
		}
		r = sys_detach(0, 0, TWZ_DETACH_ONBECOME | TWZ_DETACH_ALL);
		if(r) {
			debug_printf("failed to detach: %d", r);
			twz_thread_exit();
		}

		debug_printf("ATTACH\n");
		r = sys_attach(0, si, KSO_SECCTX);
		if(r) {
			debug_printf("failed to attach: %d", r);
			twz_thread_exit();
		}
	}
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

	unsetenv("BSNAME");
	struct thread tthr;
	int r;
	if((r = twz_thread_spawn(
	      &tthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = "term.text" }))) {
		debug_printf("failed to spawn terminal");
		abort();
	}

	twz_thread_wait(1, (struct thread *[]){ &tthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

	objid_t si;
	r = twz_name_resolve(NULL, "init.sctx", NULL, 0, &si);
	if(r) {
		debug_printf("failed to resolve 'init.sctx'");
		twz_thread_exit();
	}
	debug_printf("ATTACH\n");
	r = sys_attach(0, si, KSO_SECCTX);
	if(r) {
		debug_printf("failed to attach: %d", r);
		twz_thread_exit();
	}

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
	      &shthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = "login.text" }))) {
		debug_printf("failed to spawn shell");
		abort();
	}

	printf("DONE\n");
	twz_thread_exit();

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
