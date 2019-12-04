#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <twz/_kso.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/thread.h>
#include <twz/user.h>
#include <unistd.h>

extern char **environ;
void tmain(const char *username)
{
	char userpath[1024];
	snprintf(userpath, 512, "%s.user", username);

	objid_t uid;
	int r;
	r = twz_name_resolve(NULL, userpath, NULL, 0, &uid);
	if(r) {
		fprintf(stderr, "failed to resolve '%s'\n", userpath);
		exit(1);
	}

	twzobj user;
	twz_object_init_guid(&user, uid, FE_READ);
	struct user_hdr *uh = twz_object_base(&user);

	char userstring[128];
	snprintf(userstring, 128, IDFMT, IDPR(uid));
	setenv("TWZUSER", userstring, 1);
	setenv("USER", username, 1);
	char *ps1 = NULL;
	asprintf(&ps1, "\\e[36m\\u\\e[m@\\e[35m\\h\\e[m:\\e[32m\\w\\e[m $ ");
	setenv("PS1", ps1, 1);

	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		fprintf(stderr, "failed to detach from login context\n");
		exit(1);
	}
	if(uh->dfl_secctx) {
		r = sys_attach(0, uh->dfl_secctx, 0, KSO_SECCTX);
		if(r) {
			fprintf(stderr, "failed to attach to user context\n");
			exit(1);
		}
	}

	kso_set_name(NULL, "[instance] shell [user %s]", username);
	r = execv("/usr/bin/bash", (char *[]){ "/usr/bin/bash", NULL });
	fprintf(stderr, "failed to exec shell: %d", r);
	exit(1);
}

#include <twz/tx.h>

struct foo {
	int x, y;
	struct twz_tx tx;
};

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

int main()
{
	twzobj o;
	int r;
	if((r = twz_object_new(&o, NULL, NULL, TWZ_OC_DFL_WRITE | TWZ_OC_DFL_READ))) {
		abort();
	}
	struct foo *f = twz_object_base(&o);
	f->tx.logsz = 1024;
	f->x = f->y = 0;

	long long start = rdtsc();
	for(volatile long i = 0; i < 100000; i++) {
#if 0
		TXSTART(&o, &f->tx)
		{
			TXRECORD(&f->tx, &f->x);
			TXRECORD(&f->tx, &f->y);
			f->x = 5;
			f->y = 6;
			// TXABORT(<errcode>);
			TXCOMMIT;
		}
		TXEND;
#else
		f->x = 5;
		f->y = 6;
		_clwb(&f->x);
		_clwb(&f->y);
		_pfence();
#endif
	}
	long long end = rdtsc();

	fprintf(stderr, "OK: %d %d %d %ld\n", f->x, f->y, errno, (end - start) / 100000);
	for(;;)
		;

	printf("Setting SCE to AUX.\n");
	for(;;) {
		char buffer[1024];
		printf("Twizzler Login: ");
		fflush(NULL);
		fgets(buffer, 1024, stdin);

		char *n = strchr(buffer, '\n');
		if(n)
			*n = 0;
		if(n == buffer) {
			printf("AUTO LOGIN: bob\n");
			strcpy(buffer, "bob");
		}

		pid_t pid;
		if(!(pid = fork())) {
			tmain(buffer);
		}
		if(pid == -1) {
			warn("fork");
			continue;
		}

		int status;
		pid_t wp;
		while((wp = wait(&status)) != pid) {
			if(wp < 0) {
				if(errno == EINTR) {
					continue;
				}
				warn("wait");
				break;
			}
		}
	}
}
