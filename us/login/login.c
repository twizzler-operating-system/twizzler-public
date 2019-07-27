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
	char *username = twz_ptr_lea(twz_stdstack, a);
	struct metainfo *mi = twz_object_meta(twz_stdstack);
	struct fotentry *fe = (void *)((char *)mi + mi->milen);
	char userpath[1024];
	snprintf(userpath, 512, "%s.user", username);

	objid_t uid;
	int r;
	r = twz_name_resolve(NULL, userpath, NULL, 0, &uid);
	if(r) {
		printf("failed to resolve '%s'\n", userpath);
		twz_thread_exit();
	}

	struct object user;
	twz_object_open(&user, uid, FE_READ);
	struct user_hdr *uh = twz_obj_base(&user);

	char userstring[128];
	snprintf(userstring, 128, IDFMT, IDPR(uid));
	setenv("TWZUSER", userstring, 1);
	setenv("USER", username, 1);

	r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
	if(r) {
		fprintf(stderr, "failed to detach from login context\n");
		twz_thread_exit();
	}
	if(uh->dfl_secctx) {
		r = sys_attach(0, uh->dfl_secctx, 0, KSO_SECCTX);
		if(r) {
			fprintf(stderr, "failed to attach to user context\n");
			twz_thread_exit();
		}
	}

	char reprname[1024];
	snprintf(reprname, 1024, "[instance] shell [user %s]", username);
	twz_name_assign(twz_thread_repr_base()->reprid, reprname);

	r = execv("shell.text", (char *[]){ "shell.text", NULL });
	fprintf(stderr, "failed to exec shell: %d", r);
	twz_thread_exit();
}

#include <pthread.h>
void *_sf(void *a)
{
	debug_printf("Hello from pthread! %p\n", a);
	debug_printf("--> %s\n", a);

	return NULL;
}

int main(int argc, char **argv)
{
	for(;;) {
		char buffer[1024];
		printf("Twizzler Login: ");
		fflush(NULL);
		fgets(buffer, 1024, stdin);

		char *n = strchr(buffer, '\n');
		if(n)
			*n = 0;
		if(n == buffer)
			continue;

		struct thread tthr;
		int r;
		if((r = twz_thread_spawn(
		      &tthr, &(struct thrd_spawn_args){ .start_func = tmain, .arg = buffer }))) {
			fprintf(stderr, "failed to spawn thread\n");
		} else {
			twz_thread_wait(1, (struct thread *[]){ &tthr }, (int[]){ THRD_SYNC_EXIT }, NULL, NULL);
		}
	}
}
