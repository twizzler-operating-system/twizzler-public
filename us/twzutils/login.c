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
	// execv("/usr/bin/ycsbc",
	// (char *[]){ "ycsbc", "-db", "sqlite", "-P", "/usr/share/ycsbc/workloadf.spec", NULL });
	r = execv("/usr/bin/bash", (char *[]){ "/usr/bin/bash", NULL });
	fprintf(stderr, "failed to exec shell: %d", r);
	exit(1);
}

#if 0
#include <backtrace-supported.h>
#include <backtrace.h>

struct bt_ctx {
	struct backtrace_state *state;
	int error;
};

void foo()
{
	struct backtrace_state *state =
	  backtrace_create_state(NULL, BACKTRACE_SUPPORTS_THREADS, error_callback, NULL);
	struct bt_ctx ctx = { state, 0 };
	backtrace_print(state, 0, stdout);
	// backtrace_simple(state, 0, simple_callback, error_callback, &ctx);
}
#endif

__attribute__((used)) void foo2()
{
	printf("Hi\n");
	libtwz_do_backtrace();
}

asm(".global foo;\
	foo:;\
	.cfi_startproc;\
	.cfi_def_cfa rsp, 8;\
		push %rbp;\
	.cfi_def_cfa rsp, 16;\
		mov %rsp, %rbp;\
		call foo2;\
		pop %rbp;\
	.cfi_def_cfa rsp, 8;\
		retq;\
		.cfi_endproc;\
		");

extern void foo();
void bar()
{
	fprintf(stderr, "CALLED BACKTRACE\n");
	foo();
	// libtwz_do_backtrace();
	fprintf(stderr, "FAULTED BACKTRACE\n");
	asm volatile("hlt");
}

#include <setjmp.h>
#include <twz/fault.h>
_Thread_local jmp_buf *_jmp_buf = NULL;

void _fault_handler(int f, void *d, void *ud)
{
	if(!_jmp_buf) {
		fprintf(stderr, "UNCAUGHT EXCEPTION\n");
		twz_thread_exit(1);
	}
	longjmp(*_jmp_buf, f + 1);
}

#include <twz/twztry.h>
void try_test(void)
{
	twztry
	{
		printf("In try!\n");
		twztry
		{
			printf("In inner try\n");
			//	int *p = NULL;
			//	volatile int x = *p;
			//		twz_fault_raise(FAULT_OBJECT, NULL);
			void *p = 0x8000000001000;
			twzobj o0 = twz_object_from_ptr(NULL);
			void *v = twz_object_lea(&o0, p);
		}
		twztry_end;
	}
	twzcatch_all
	{
		//		struct fault_null_info *fni = twzcatch_data();
		printf("In catch:: %d %lx\n", twzcatch_fnum(), 0);
	}
	twztry_end;
	printf("After try block\n");

#if 0
	jmp_buf jb;
	jmp_buf *pj = _jmp_buf;
	_jmp_buf = &jb;

	if(!setjmp(*_jmp_buf)) {
		fprintf(stderr, "in try\n");

		jmp_buf jb;
		jmp_buf *pj = _jmp_buf;
		_jmp_buf = &jb;

		if(!setjmp(*_jmp_buf)) {
			fprintf(stderr, "in try inner\n");

			int *p = NULL;
			volatile int x = *p;

		} else {
			fprintf(stderr, "in catch inner\n");
			longjmp(*pj, 1);
		}

		_jmp_buf = pj;

	} else {
		fprintf(stderr, "in catch\n");
	}

	_jmp_buf = pj;
#endif
}

int main(int argc, char **argv)
{
	try_test();
	// int k = 0x7fffffff;
	//	k += argc;
	printf("Setting SCE to AUX.\n\n");
	//	bar();
	for(;;) {
		char buffer[1024];
		printf("Twizzler Login: ");
		fflush(NULL);
		fgets(buffer, 1024, stdin);
		// strcpy(buffer, "bob");

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
		for(volatile long i = 0; i < 100000000; i++) {
		}
	}
}
