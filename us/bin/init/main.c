#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/driver/bus.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>

int __name_bootstrap(void);

struct service_info {
	objid_t sctx;
	char *name;
	char arg[1024];
	char arg2[1024];
};
int create_pty_pair(char *server, char *client);

bool term_ready = false;

#define EPRINTF(...) ({ term_ready ? fprintf(stderr, ##__VA_ARGS__) : debug_printf(__VA_ARGS__); })
void tmain(void *a)
{
	struct service_info *info = twz_object_lea(twz_stdstack, a);
	int r;

	char buffer[1024];
	snprintf(buffer, 1024, "%s.text", info->name);

	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		EPRINTF("failed to detach: %d\n", r);
		twz_thread_exit(r);
	}

	if(info->sctx) {
		r = sys_attach(0, info->sctx, 0, KSO_SECCTX);
		if(r) {
			EPRINTF("failed to attach " IDFMT ": %d\n", IDPR(info->sctx), r);
			twz_thread_exit(r);
		}
	}

	kso_set_name(NULL, "[instance] %s", info->name);
	r = execv(buffer,
	  (char *[]){
	    info->name, info->arg[0] ? info->arg : NULL, info->arg2[0] ? info->arg2 : NULL, NULL });
	EPRINTF("failed to 1exec '%s': %d\n", info->name, r);
	twz_thread_exit(r);
}

void start_service(struct service_info *info)
{
	int r;
	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		EPRINTF("failed to detach: %d\n", r);
		twz_thread_exit(r);
	}

	if(info->sctx) {
		r = sys_attach(0, info->sctx, 0, KSO_SECCTX);
		if(r) {
			EPRINTF("failed to attach " IDFMT ": %d\n", IDPR(info->sctx), r);
			twz_thread_exit(r);
		}
	}

	kso_set_name(NULL, "[instance] %s", info->name);
	r = execv(info->name,
	  (char *[]){
	    info->name, info->arg[0] ? info->arg : NULL, info->arg2[0] ? info->arg2 : NULL, NULL });
	EPRINTF("failed to 2exec '%s': %d\n", info->name, r);
	exit(1);
}

void start_terminal(char *input, char *output, char *pty)
{
	kso_set_name(NULL, "[instance] term");
	execv(
	  "/usr/bin/term", (char *[]){ "/usr/bin/term", "-i", input, "-o", output, "-p", pty, NULL });
	// EPRINTF("failed to exec '%s': %s\n", strerror(errno));
	EPRINTF("failed to exec /usr/bin/term\n");
	exit(1);
}

void start_login(void)
{
	objid_t lsi;
	int r = twz_name_resolve(NULL, "usr_bin_login.sctx", NULL, 0, &lsi);
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
		twz_thread_exit(r);
	}

	kso_set_name(NULL, "[instance] login");

	r = execv("/usr/bin/login", (char *[]){ "/usr/bin/login", NULL });
	EPRINTF("execv failed: %d\n", r);
}

#include <twz/view.h>

pthread_cond_t logging_ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logging_ready_lock = PTHREAD_MUTEX_INITIALIZER;

void *logmain(void *arg)
{
	kso_set_name(NULL, "[instance] init-logger");
	// objid_t *lid = twz_object_lea(twz_stdstack, arg);
	objid_t *lid = arg;

	twzobj sobj;
	twz_object_init_guid(&sobj, *lid, FE_READ | FE_WRITE);

	pthread_mutex_lock(&logging_ready_lock);
	pthread_cond_signal(&logging_ready);
	pthread_mutex_unlock(&logging_ready_lock);

	// twz_thread_ready(NULL, THRD_SYNC_READY, 1);
	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = bstream_read(&sobj, buf, 127, 0);
		__sys_debug_print(buf, r);
	}
	return NULL;
}

#include <twz/pty.h>

int create_pty_pair(char *server, char *client)
{
	twzobj pty_s, pty_c;

	int r;
	if((r = twz_object_new(&pty_s,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = twz_object_new(&pty_c,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = pty_obj_init_server(&pty_s, twz_object_base(&pty_s))))
		return r;

	struct pty_hdr *ps = twz_object_base(&pty_s);
	if((r = pty_obj_init_client(&pty_c, twz_object_base(&pty_c), ps)))
		return r;

	if((r = twz_name_assign(twz_object_guid(&pty_s), server))) {
		return r;
	}

	if((r = twz_name_assign(twz_object_guid(&pty_c), client))) {
		return r;
	}

	return 0;
}

#include <twz/hier.h>
twzobj bs;
void init_test_iter(void);
void init_test_init(void);
static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#include <twz/btree.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
	return;
}

void slab_test();

int main()
{
	struct timespec start, end, diff;
	long max = 1000000;
	int k;
	twzobj o;
	(void)k;
	(void)o;
	(void)max;
	(void)diff;
	(void)start;
	(void)end;
	int r;

	kso_set_name(NULL, "[instance] init");

	objid_t lid;
	twzobj lobj;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &lid))) {
		EPRINTF("failed to create log object");
		abort();
	}
	if((r = twz_object_init_guid(&lobj, lid, FE_READ | FE_WRITE))) {
		EPRINTF("failed to open log object");
		abort();
	}
	if((r = twz_name_assign(lid, "dev:dfl:log"))) {
		EPRINTF("failed to assign log object name");
		abort();
	}

	if((r = bstream_obj_init(&lobj, twz_object_base(&lobj), 16))) {
		EPRINTF("failed to init log bstream");
		abort();
	}

	objid_t nid;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &nid))) {
		EPRINTF("failed to create null object");
		abort();
	}
	if((r = twz_name_assign(nid, "dev:null"))) {
		EPRINTF("failed to assign null object name");
		abort();
	}

	pthread_t logging_thread;
	pthread_create(&logging_thread, NULL, logmain, &lid);

	pthread_mutex_lock(&logging_ready_lock);
	pthread_cond_wait(&logging_ready, &logging_ready_lock);
	pthread_mutex_unlock(&logging_ready_lock);
#if 0
	struct thread lthr;
	if((r = twz_thread_spawn(
	      &lthr, &(struct thrd_spawn_args){ .start_func = logmain, .arg = &lid }))) {
		EPRINTF("failed to spawn logger");
		abort();
	}

	

	if(twz_thread_wait(1, (struct thread *[]){ &lthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL)
	   < 0) {
		EPRINTF("failed to wait for logging thread\n");
		abort();
	}
#endif
	objid_t si;
	r = twz_name_resolve(NULL, "usr_bin_init.sctx", NULL, 0, &si);
	if(r) {
		EPRINTF("failed to resolve 'init.sctx'\n");
		twz_thread_exit(r);
	}
	r = sys_attach(0, si, 0, KSO_SECCTX);
	if(r) {
		EPRINTF("failed to attach: %d\n", r);
		twz_thread_exit(r);
	}

	int fd;
	close(0);
	close(1);
	close(2);
	if((fd = open("dev:null", O_RDONLY)) != 0) {
		EPRINTF("err opening stdin\n");
		abort();
	}
	if((fd = open("dev:dfl:log", O_RDWR)) != 1) {
		EPRINTF("err opening stdout\n");
		abort();
	}
	if((fd = open("dev:dfl:log", O_RDWR)) != 2) {
		EPRINTF("err opening stderr\n");
		abort();
	}

	term_ready = true;

	if(!fork()) {
		execlp("twzdev", "twzdev", NULL);
	}
	int status;
	wait(&status);

	if((r = create_pty_pair("dev:pty:pty0", "dev:pty:ptyc0"))) {
		EPRINTF("failed to create pty pair\n");
		abort();
	}
	if(!fork()) {
		start_terminal("dev:input:keyboard", "dev:output:framebuffer", "dev:pty:pty0");
	}

	if(!fork()) {
		close(0);
		close(1);
		close(2);

		if((fd = open("dev:pty:ptyc0", O_RDONLY)) != 0) {
			EPRINTF("err opening stdin: %d\n", fd);
			abort();
		}
		if((fd = open("dev:pty:ptyc0", O_RDWR)) != 1) {
			EPRINTF("err opening stdout\n");
			abort();
		}
		if((fd = open("dev:pty:ptyc0", O_RDWR)) != 2) {
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

		if((fd = open("dev:pty:ptyS0c", O_RDONLY)) != 0) {
			EPRINTF("1err opening stdin: %d\n", fd);
			abort();
		}
		if((fd = open("dev:pty:ptyS0c", O_RDWR)) != 1) {
			EPRINTF("1err opening stdout\n");
			abort();
		}
		if((fd = open("dev:pty:ptyS0c", O_RDWR)) != 2) {
			EPRINTF("1err opening stderr\n");
			abort();
		}

		start_login();
		exit(0);
	}

	exit(0);
}
