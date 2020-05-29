#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/driver/bus.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <unistd.h>

bool term_ready = false;

#define EPRINTF(...) ({ term_ready ? fprintf(stderr, ##__VA_ARGS__) : debug_printf(__VA_ARGS__); })

void start_terminal(char *input, char *output, char *pty)
{
	kso_set_name(NULL, "[instance] term");
	execv(
	  "/usr/bin/term", (char *[]){ "/usr/bin/term", "-i", input, "-o", output, "-p", pty, NULL });
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
		exit(1);
	}

	kso_set_name(NULL, "[instance] login");

	execv("/usr/bin/login", (char *[]){ "/usr/bin/login", NULL });
	exit(1);
}

pthread_cond_t logging_ready = PTHREAD_COND_INITIALIZER;
pthread_mutex_t logging_ready_lock = PTHREAD_MUTEX_INITIALIZER;

void *logmain(void *arg)
{
	kso_set_name(NULL, "[instance] init-logger");
	objid_t *lid = arg;

	twzobj sobj;
	twz_object_init_guid(&sobj, *lid, FE_READ | FE_WRITE);

	pthread_mutex_lock(&logging_ready_lock);
	pthread_cond_signal(&logging_ready);
	pthread_mutex_unlock(&logging_ready_lock);

	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = bstream_read(&sobj, buf, 127, 0);
		__sys_debug_print(buf, r);
	}
	return NULL;
}

void reopen(const char *in, const char *out, const char *err)
{
	close(0);
	close(1);
	close(2);
	if(open(in, O_RDWR) != 0)
		EPRINTF("failed to open `%s' as stdin\n", in);
	if(open(out, O_RDWR) != 1)
		EPRINTF("failed to open `%s' as stdout\n", out);
	if(open(err, O_RDWR) != 2)
		EPRINTF("failed to open `%s' as stderr\n", err);
}

int main()
{
	int r;
	kso_set_name(NULL, "[instance] init");

	/* start-off by ensuring the directory structure is sane */
	if(mkdir("/dev", 0700) == -1) {
		if(errno != EEXIST) {
			EPRINTF("failed to make /dev\n");
			return 1;
		}
	}
	if(mkdir("/dev/raw", 0700) == -1) {
		if(errno != EEXIST) {
			EPRINTF("failed to make /dev/raw\n");
			return 1;
		}
	}
	if(mkdir("/dev/pty", 0700) == -1) {
		if(errno != EEXIST) {
			EPRINTF("failed to make /dev/pty\n");
			return 1;
		}
	}

	twzobj lobj;
	if((r = twz_object_new(&lobj,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		EPRINTF("failed to create log object\n");
		abort();
	}

	if((r = twz_name_assign(twz_object_guid(&lobj), "/dev/init-log"))) {
		EPRINTF("failed to assign log object name\n");
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
	if((r = twz_name_assign(nid, "/dev/null"))) {
		EPRINTF("failed to assign null object name");
		abort();
	}

	pthread_t logging_thread;
	objid_t lid = twz_object_guid(&lobj);
	pthread_create(&logging_thread, NULL, logmain, &lid);

	pthread_mutex_lock(&logging_ready_lock);
	pthread_cond_wait(&logging_ready, &logging_ready_lock);
	pthread_mutex_unlock(&logging_ready_lock);

	/* logging thread has signaled that it's ready */
	reopen("/dev/null", "/dev/init-log", "/dev/init-log");
	term_ready = true;

	objid_t si;
	r = twz_name_resolve(NULL, "usr_bin_init.sctx", NULL, 0, &si);
	if(r) {
		EPRINTF("failed to resolve 'init.sctx'\n");
		exit(1);
	}
	r = sys_attach(0, si, 0, KSO_SECCTX);
	if(r) {
		EPRINTF("failed to attach: %d\n", r);
		exit(1);
	}

	/* start the device manager */
	if(!fork()) {
		execlp("twzdev", "twzdev", NULL);
		exit(0);
	}

	int status;
	r = wait(&status);

	/* start the terminal program */
	if(access("/dev/pty0", F_OK) == 0) {
		if(!fork()) {
			start_terminal("/dev/keyboard", "/dev/framebuffer", "/dev/pty/pty0");
		}
	} else {
		fprintf(stderr, "no supported framebuffer found; skipping starting terminal: %d\n", errno);
	}

	/* start a login on the serial port and the terminal */
	if(access("/dev/ptyc0", F_OK) == 0) {
		if(!fork()) {
			reopen("/dev/pty/ptyc0", "/dev/pty/ptyc0", "/dev/pty/ptyc0");

			start_login();
		}
	} else {
		fprintf(
		  stderr, "no supported framebuffer found; skipping starting login shell on terminal\n");
	}

	if(!fork()) {
		reopen("/dev/pty/ptyS0c", "/dev/pty/ptyS0c", "/dev/pty/ptyS0c");

		start_login();
	}

	exit(0);
}
