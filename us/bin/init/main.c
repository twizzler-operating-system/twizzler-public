#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <twz/_sys.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/driver/memory.h>
#include <twz/hier.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/persist.h>
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

#include <twz/driver/queue.h>
#include <twz/queue.h>
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
	mkdir("/tmp", 0777);

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

	if(!fork()) {
		execlp("pager", "pager", NULL);
		exit(0);
	}
	wait(&status);

	mkdir("/tmp");
	twzobj st;
	if(twz_object_init_name(&st, "/storage", FE_READ | FE_WRITE) == 0) {
		fprintf(stderr, "[init] switching name root to storage\n");
		twzobj devdir;
		if(twz_object_init_name(&devdir, "/dev", FE_READ) == 0) {
			if(twz_name_assign_namespace(twz_object_guid(&devdir), "/storage/dev") == 0) {
				if(chroot("/storage") == -1) {
					fprintf(
					  stderr, "[init] failed to chroot to new root; continuing from initrd\n");
				}
				mkdir("/tmp", 0777);
			} else {
				fprintf(stderr, "[init] failed to link dev directory, continuing from initrd\n");
			}
		} else {
			fprintf(stderr, "[init] failed to open dev directory, continuing from initrd\n");
		}
	} else {
		fprintf(stderr, "[init] failed to switch to storage, continuing from initrd\n");
	}

	mkdir("/tmp");
	DIR *nvd = opendir("/dev/nv");
	if(nvd) {
		struct dirent *de;
		while((de = readdir(nvd))) {
			if(de->d_name[0] == '.')
				continue;
			char *path = NULL;
			asprintf(&path, "/dev/nv/%s", de->d_name);
			twzobj nvo, nvmetao;
			twz_object_init_name(&nvo, path, FE_READ);
			struct nv_header *nvhdr = twz_device_getds(&nvo);
			objid_t metaid = MKID(nvhdr->meta_hi, nvhdr->meta_lo);
			twz_object_init_guid(&nvmetao, metaid, FE_READ | FE_WRITE);
			struct nvdimm_region_header *hdr = twz_object_base(&nvmetao);

			if(hdr->magic == NVD_HDR_MAGIC) {
				if(hdr->nameroot == 0) {
					twzobj nro;
					r = twz_hier_namespace_new(
					  &nro, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_SYS_OC_PERSIST_);
					printf("[init] creating nameroot " IDFMT " for NVR %s\n",
					  IDPR(twz_object_guid(&nro)),
					  path);
					if(r) {
						fprintf(stderr, "[init] failed to create nameroot for %s\n", path);
					} else {
						hdr->nameroot = twz_object_guid(&nro);
						_clwb_len(&hdr->nameroot, sizeof(hdr->nameroot));
						_pfence();
					}
				}

				if(hdr->nameroot != 0) {
					twzobj root;
					printf("[init] 'mounting' nameroot " IDFMT " for NVR %s\n",
					  IDPR(hdr->nameroot),
					  path);
					if(twz_object_init_name(&root, "/storage", FE_READ | FE_WRITE)) {
						fprintf(stderr, "failed to open root\n");
					}
					r = twz_hier_assign_name(&root, de->d_name, NAME_ENT_NAMESPACE, hdr->nameroot);
					if(r) {
						fprintf(stderr, "[init] error 'mounting' NVR %s: %d\n", de->d_name, r);
					}
					if(twz_object_init_name(&root, "/", FE_READ | FE_WRITE)) {
						fprintf(stderr, "failed to open root\n");
					}
					r = twz_hier_assign_name(&root, de->d_name, NAME_ENT_NAMESPACE, hdr->nameroot);
					if(r) {
						fprintf(stderr, "[init] error 'mounting' NVR %s: %d\n", de->d_name, r);
					}
				}
			}

			free(path);
		}
	}

	twzobj rxqobj;
	twzobj txqobj;
	if(twz_object_new(&txqobj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE)
	   < 0)
		abort();
	if(twz_object_new(&rxqobj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE)
	   < 0)
		abort();
	queue_init_hdr(
	  &txqobj, 5, sizeof(struct queue_entry_packet), 5, sizeof(struct queue_entry_packet));
	queue_init_hdr(
	  &rxqobj, 5, sizeof(struct queue_entry_packet), 5, sizeof(struct queue_entry_packet));

	twz_name_assign(twz_object_guid(&txqobj), "/dev/e1000-rxqueue");
	twz_name_assign(twz_object_guid(&rxqobj), "/dev/e1000-txqueue");

	if(!fork()) {
		kso_set_name(NULL, "[instance] e1000-driver");
		execvp("e1000",
		  (char *[]){ "e1000", "/dev/e1000", "/dev/e1000-txqueue", "/dev/e1000-rxqueue", NULL });
		fprintf(stderr, "failed to start e1000 driver: %d\n", errno);
		exit(1);
	}

	if(0 && !fork()) {
		kso_set_name(NULL, "[instance] net *test*");
		execvp("net", (char *[]){ "net", "/dev/e1000-txqueue", "/dev/e1000-rxqueue", NULL });
		fprintf(stderr, "failed to start net test\n");
		exit(1);
	}

	/* start the terminal program */
	if(access("/dev/pty/pty0", F_OK) == 0) {
		if(!fork()) {
			start_terminal("/dev/keyboard", "/dev/framebuffer", "/dev/pty/pty0");
		}
	} else {
		fprintf(stderr, "no supported framebuffer found; skipping starting terminal: %d\n", errno);
	}

	setenv("PATH", "/bin:/usr/bin:/usr/local/bin:/opt/usr/bin", 1);

	/* start a login on the serial port and the terminal */
	if(access("/dev/pty/ptyc0", F_OK) == 0) {
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
