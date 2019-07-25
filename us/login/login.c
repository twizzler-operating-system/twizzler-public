#include <stdlib.h>
#include <string.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <twz/user.h>
#include <unistd.h>

extern char **environ;
void tmain(void *a)
{
	char *userpath = a;
	/*
	for(char **env = environ; *env != 0; env++) {
	    char *thisEnv = *env;
	    printf("%s\n", thisEnv);
	}*/

	objid_t uid;
	int r;
	r = twz_name_resolve(NULL, userpath, NULL, 0, &uid);
	if(r) {
		printf("failed to resolve '%s'\n", userpath);
		twz_thread_exit();
	}

	// printf("RESOL: " IDFMT "\n", IDPR(uid));
	struct object user;
	twz_object_open(&user, uid, FE_READ);
	struct user_hdr *uh = twz_obj_base(&user);

	// printf(":: %s :: " IDFMT "\n", twz_ptr_lea(&user, uh->name), IDPR(uh->dfl_secctx));

	r = sys_attach(0, uh->dfl_secctx, KSO_SECCTX);

	/* TODO: figure out a way to "detach from all" or something */
	objid_t isi;
	r = twz_name_resolve(NULL, "login.sctx", NULL, 0, &isi);
	if(r) {
		printf("failed to resolve 'login.sctx'");
		twz_thread_exit();
	}

	r = sys_detach(0, isi, TWZ_DETACH_ONBECOME);
	if(r) {
		printf("failed to detach: %d", r);
		twz_thread_exit();
	}

	// twz_exec(id, (const char *const[]){ pg, NULL }, NULL);
	r = execv("shell.text", (char *[]){ "shell.text", NULL });
	printf("failed to exec shell: %d", r);
	twz_thread_exit();
}
char userpath[1024];

int main(int argc, char **argv)
{
	char buffer[1024];
	for(;;) {
		printf("LOGIN> ");
		fflush(NULL);
		fgets(buffer, 1024, stdin);
		char *n = strchr(buffer, '\n');
		if(n)
			*n = 0;
		snprintf(userpath, 512, "%s.user", buffer);

		struct thread tthr;
		int r;
		if((r = twz_thread_spawn(
		      &tthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = userpath }))) {
			printf("failed to spawn thread");
			twz_thread_exit();
		}

		twz_thread_wait(1, (struct thread *[]){ &tthr }, (int[]){ THRD_SYNC_EXIT }, NULL, NULL);
	}
}
