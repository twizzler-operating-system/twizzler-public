#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>

#include <twz/driver/device.h>

int __name_bootstrap(void);

struct service_info {
	objid_t sctx;
	const char *name;
	char arg[1024];
	char arg2[1024];
};

bool term_ready = false;

#define EPRINTF(...) ({ term_ready ? fprintf(stderr, ##__VA_ARGS__) : debug_printf(__VA_ARGS__); })
void tmain(void *a)
{
	struct service_info *info = twz_ptr_lea(&twz_stdstack, a);
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
	r = execv(buffer,
	  (char *[]){
	    info->name, info->arg[0] ? info->arg : NULL, info->arg2[0] ? info->arg2 : NULL, NULL });
	EPRINTF("failed to exec '%s': %d\n", info->name, r);
	twz_thread_exit();
}

void start_login(void)
{
	objid_t lsi;
	int r = twz_name_resolve(NULL, "login.sctx", NULL, 0, &lsi);
	if(r) {
		EPRINTF("failed to resolve 'login.sctx'");
		exit(0);
	}

	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		EPRINTF("failed to detach: %d\n", r);
		exit(0);
	}

	r = sys_attach(0, lsi, 0, KSO_SECCTX);
	if(r) {
		EPRINTF("failed to attach " IDFMT ": %d\n", IDPR(lsi), r);
		twz_thread_exit();
	}

	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] login");

	r = execv("login.text", (char *[]){ "login", NULL });
	EPRINTF("execv failed: %d\n", r);
}

#include <twz/view.h>
void logmain(void *arg)
{
	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] init-logger");
	objid_t *lid = twz_ptr_lea(&twz_stdstack, arg);

	objid_t target;
	twz_vaddr_to_obj(lid, &target, NULL);
	struct object sobj;
	twz_object_open(&sobj, *lid, FE_READ | FE_WRITE);
	twz_thread_ready(NULL, THRD_SYNC_READY, 1);
	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = bstream_read(&sobj, buf, 127, 0);
		__sys_debug_print(buf, r);
	}
}

struct object bs;
int main(int argc, char **argv)
{
	debug_printf("Bootstrapping naming system\n");
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

	objid_t lid;
	struct object lobj;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &lid))) {
		debug_printf("failed to create log object");
		abort();
	}
	if((r = twz_object_open(&lobj, lid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open log object");
		abort();
	}
	if((r = twz_name_assign(lid, "dev:dfl:log"))) {
		debug_printf("failed to assign log object name");
		abort();
	}

	if((r = bstream_obj_init(&lobj, twz_obj_base(&lobj), 16))) {
		debug_printf("failed to init log bstream");
		abort();
	}

	objid_t nid;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &nid))) {
		debug_printf("failed to create null object");
		abort();
	}
	if((r = twz_name_assign(nid, "dev:null"))) {
		debug_printf("failed to assign null object name");
		abort();
	}

	struct thread lthr;
	if((r = twz_thread_spawn(
	      &lthr, &(struct thrd_spawn_args){ .start_func = logmain, .arg = &lid }))) {
		EPRINTF("failed to spawn logger");
		abort();
	}

	twz_thread_wait(1, (struct thread *[]){ &lthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);
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
	if((fd = open("dev:null", O_RDONLY)) != 0) {
		EPRINTF("err opening stdin");
		abort();
	}
	if((fd = open("dev:dfl:log", O_RDWR)) != 1) {
		EPRINTF("err opening stdout");
		abort();
	}
	if((fd = open("dev:dfl:log", O_RDWR)) != 2) {
		EPRINTF("err opening stderr");
		abort();
	}

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
		struct thread *dt = malloc(sizeof(*dt));
		switch(k->type) {
			struct object dobj;
			case KSO_DEVBUS:
				sprintf(drv_info.arg, IDFMT, IDPR(k->id));
				drv_info.name = "pcie";
				/* TODO: determine the type of bus, and start something appropriate */
				if((r = twz_thread_spawn(
				      dt, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &drv_info }))) {
					EPRINTF("failed to spawn driver");
					abort();
				}
				twz_thread_wait(
				  1, (struct thread *[]){ dt }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

				break;
			case KSO_DEVICE:
				twz_object_open(&dobj, k->id, FE_READ);

				objid_t uid;
				twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &uid);
				sprintf(drv_info.arg2, IDFMT, IDPR(uid));

				struct object stream;
				twz_object_open(&stream, uid, FE_READ | FE_WRITE);

				if((r = bstream_obj_init(&stream, twz_obj_base(&stream), 16))) {
					debug_printf("failed to init bstream");
					abort();
				}

				struct device_repr *dr = twz_obj_base(&dobj);
				if(dr->device_type == DEVICE_INPUT) {
					sprintf(drv_info.arg, IDFMT, IDPR(k->id));
					drv_info.name = "input";
					if((r = twz_thread_spawn(dt,
					      &(struct thrd_spawn_args){ .start_func = tmain, .arg = &drv_info }))) {
						EPRINTF("failed to spawn driver");
						abort();
					}
					twz_thread_wait(
					  1, (struct thread *[]){ dt }, (int[]){ THRD_SYNC_READY }, NULL, NULL);
					twz_name_assign(uid, "dev:input:keyboard");
				}
				if(dr->device_type == DEVICE_IO && dr->device_type == DEVICE_ID_SERIAL) {
					sprintf(drv_info.arg, IDFMT, IDPR(k->id));
					drv_info.name = "serial";
					if((r = twz_thread_spawn(dt,
					      &(struct thrd_spawn_args){ .start_func = tmain, .arg = &drv_info }))) {
						EPRINTF("failed to spawn driver");
						abort();
					}
					twz_thread_wait(
					  1, (struct thread *[]){ dt }, (int[]){ THRD_SYNC_READY }, NULL, NULL);
					twz_name_assign(uid, "dev:input:serial");
				}

				break;
		}
	}

	if((r = twz_thread_spawn(
	      &tthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &term_info }))) {
		EPRINTF("failed to spawn terminal");
		abort();
	}
	twz_thread_wait(1, (struct thread *[]){ &tthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);
	term_ready = true;
	EPRINTF("twzinit: terminal ready\n");

	if(!fork()) {
		close(0);
		close(1);
		close(2);

		if((fd = open("dev:dfl:input", O_RDONLY)) != 0) {
			EPRINTF("err opening stdin: %d\n", fd);
			abort();
		}
		if((fd = open("dev:dfl:screen", O_RDWR)) != 1) {
			EPRINTF("err opening stdout\n");
			abort();
		}
		if((fd = open("dev:dfl:screen", O_RDWR)) != 2) {
			EPRINTF("err opening stderr\n");
			abort();
		}

		start_login();
		exit(0);
	}

	if(!fork()) {
		close(0);
		close(1);
		close(2);

		if((fd = open("dev:input:serial", O_RDONLY)) != 0) {
			EPRINTF("err opening stdin: %d\n", fd);
			abort();
		}
		if((fd = open("dev:output:serial", O_RDWR)) != 1) {
			EPRINTF("err opening stdout\n");
			abort();
		}
		if((fd = open("dev:output:serial", O_RDWR)) != 2) {
			EPRINTF("err opening stderr\n");
			abort();
		}

		start_login();
		exit(0);
	}

	exit(0);
}
