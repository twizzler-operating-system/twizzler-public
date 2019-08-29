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
	char arg[1024];
};

bool term_ready = false;

#define EPRINTF(...) ({ term_ready ? fprintf(stderr, ##__VA_ARGS__) : debug_printf(__VA_ARGS__); })
void tmain(void *a)
{
	struct service_info *info = twz_ptr_lea(twz_stdstack, a);
	int r;

	char buffer[1024];
	snprintf(buffer, 1024, "%s.text", info->name);

	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		EPRINTF("failed to detach: %d\n", r);
		twz_thread_exit();
	}

	if(info->sctx) {
		r = sys_attach(0, info->sctx, 0, KSO_SECCTX);
		if(r) {
			EPRINTF("failed to attach " IDFMT ": %d\n", IDPR(info->sctx), r);
			twz_thread_exit();
		}
	}

	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] %s", info->name);
	r = execv(buffer, (char *[]){ info->name, info->arg[0] ? info->arg : NULL, NULL });
	EPRINTF("failed to exec '%s': %d\n", info->name, r);
	twz_thread_exit();
}

struct object bs;
int main(int argc, char **argv)
{
	if(__name_bootstrap() == -1) {
		EPRINTF("Failed to bootstrap namer\n");
		abort();
	}
	unsetenv("BSNAME");

	struct thread tthr;
	int r;

	struct service_info term_info = {
		.name = "term",
		.sctx = 0,
	};

	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] init");

	if((r = twz_thread_spawn(
	      &tthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &term_info }))) {
		EPRINTF("failed to spawn terminal");
		abort();
	}
	twz_thread_wait(1, (struct thread *[]){ &tthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

	objid_t si;
	r = twz_name_resolve(NULL, "init.sctx", NULL, 0, &si);
	if(r) {
		EPRINTF("failed to resolve 'init.sctx'");
		twz_thread_exit();
	}
	r = sys_attach(0, si, 0, KSO_SECCTX);
	if(r) {
		EPRINTF("failed to attach: %d", r);
		twz_thread_exit();
	}

	int fd;
	if((fd = open("dev:dfl:keyboard", O_RDONLY)) != 0) {
		EPRINTF("err opening stdin");
		abort();
	}
	if((fd = open("dev:dfl:screen", O_RDWR)) != 1) {
		EPRINTF("err opening stdout");
		abort();
	}
	if((fd = open("dev:dfl:screen", O_RDWR)) != 2) {
		EPRINTF("err opening stderr");
		abort();
	}

	term_ready = true;
	EPRINTF("twzinit: terminal ready\n");

	/* start devices */
	struct object root;
	twz_object_open(&root, 1, FE_READ);

	struct kso_root_repr *rr = twz_obj_base(&root);
	struct service_info drv_info = {
		.name = "pcie",
		.sctx = 0,
	};

	for(size_t i = 0; i < rr->count; i++) {
		struct kso_attachment *k = &rr->attached[i];
		if(!k->id || !k->type)
			continue;
		switch(k->type) {
			struct thread dt;
			case KSO_DEVBUS:
				sprintf(drv_info.arg, IDFMT, IDPR(k->id));
				/* TODO: determine the type of bus, and start something appropriate */
				if((r = twz_thread_spawn(
				      &dt, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &drv_info }))) {
					EPRINTF("failed to spawn driver");
					abort();
				}
				twz_thread_wait(
				  1, (struct thread *[]){ &dt }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

				break;
		}
	}

	objid_t lsi;
	r = twz_name_resolve(NULL, "login.sctx", NULL, 0, &lsi);
	if(r) {
		EPRINTF("failed to resolve 'login.sctx'");
		twz_thread_exit();
	}

	struct service_info login_info = {
		.name = "login",
		.sctx = lsi,
	};

	EPRINTF("twzinit: starting login program\n");
	struct thread shthr;
	if((r = twz_thread_spawn(
	      &shthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &login_info }))) {
		EPRINTF("failed to spawn shell");
		abort();
	}

	EPRINTF("twzinit: init process completed\n");
	twz_thread_exit();

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
