#include <stdio.h>
#include <stdbool.h>

#include <twzname.h>
#include <twzexec.h>
#include <twzthread.h>

struct exinfo {
	objid_t eid;
	char **argv;
};

static char *argv[128] = {0};

static void _thrd_exec(void *arg)
{
	struct exinfo *ei = __twz_ptr_lea(&stdobj_thrd, arg);
	//twz_exec(*ei, 0);
	twz_execv(ei->eid, 0, ei->argv);
}

static void runcmd(char *cmd)
{
	char *prog = strtok(cmd, " ");
	argv[0] = strdup(prog);
	for(int i=1;i<120;i++) {
		char *arg = argv[i] = strtok(NULL, " ");
		if(!arg) {
			break;
		}
		argv[i] = strdup(arg);
	}

	char exec[256];
	snprintf(exec, 256, "%s.0", prog);
	objid_t eid = twz_name_resolve(NULL, exec, NAME_RESOLVER_DEFAULT);
	if(!eid) {
		snprintf(exec, 256, "utils/%s.0", prog);
		eid = twz_name_resolve(NULL, exec, NAME_RESOLVER_DEFAULT);
	}
	if(eid) {
		struct twzthread thrd;
		struct exinfo ei = {
			.eid = eid,
			.argv = argv,
		};
		if(twz_thread_spawn(&thrd, _thrd_exec, NULL, &ei, 0) < 0) {
			fprintf(stderr, "Failed to spawn child thread");
			return 1;
		}
		twz_thread_wait(&thrd);
	} else {
		fprintf(stderr, "No command '%s'\n", cmd);
	}

	for(int i=0;i<120;i++) {
		if(argv[i]) {
			free(argv[i]);
		}
	}
}

int main()
{
	printf("Hello, World! (shell)\n");
	char buf[128];
	for(;;) {
		fprintf(stderr, "twz $ ");
		if(fgets(buf, 127, stdin)) {
			char *nl = strchr(buf, '\n');
			if(nl) *nl = 0;
			if(nl == buf) continue;
			//fprintf(stderr, "-> %s\n", buf);
			runcmd(buf);
		}
	}
	for(;;);
}

