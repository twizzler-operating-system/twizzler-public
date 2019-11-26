#include <stdlib.h>
#include <sys/wait.h>
#include <twz/_err.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>

const char *kso_names[] = {
	[KSO_ROOT] = "root",
	[KSO_THREAD] = "thread",
	[KSO_VIEW] = "view",
	[KSO_SECCTX] = "secctx",
	[KSO_DEVBUS] = "devbus",
	[KSO_DEVICE] = "device",
};

void print_kat(struct kso_attachment *k, int indent)
{
	printf("%*s", indent, "");
	if(k->type >= KSO_MAX) {
		printf("[unknown] ");
	} else {
		printf("[%s] ", kso_names[k->type]);
	}

	twzobj obj;
	twz_object_open(&obj, k->id, FE_READ);
	struct kso_hdr *hdr = twz_object_base(&obj);

	printf("%s", hdr->name);
	printf("\n");
}

void kls_thread(struct kso_attachment *p, int indent)
{
	twzobj thr;
	twz_object_open(&thr, p->id, FE_READ);
	struct twzthread_repr *r = twz_object_base(&thr);
	for(int i = 0; i < TWZ_THRD_MAX_SCS; i++) {
		struct kso_attachment *k = &r->attached[i];
		if(k->id == 0 || k->type != KSO_SECCTX)
			continue;
		print_kat(k, indent);
	}
}

void kls(void)
{
	twzobj root;
	twz_object_open(&root, 1, FE_READ);

	struct kso_root_repr *r = twz_object_base(&root);
	for(size_t i = 0; i < r->count; i++) {
		struct kso_attachment *k = &r->attached[i];
		if(!k->id || !k->type)
			continue;
		print_kat(k, 0);
		switch(k->type) {
			case KSO_THREAD:
				kls_thread(k, 4);
				break;
		}
	}
}

extern char **environ;

void tmain(char *cmd)
{
	/* TODO: add and remove instance */
	while(*cmd == ' ') {
		*cmd++ = 0;
	}
	char *args[1024];
	int c = 0;

	char *sp = cmd;
	char *prev = cmd;
	while((sp = strchr(sp, ' '))) {
		while(*sp == ' ') {
			*sp++ = 0;
		}

		args[c++] = prev;
		prev = sp;
	}
	if(*prev) {
		args[c++] = prev;
	}
	args[c] = NULL;

	char buf[1024];
	sprintf(buf, "/usr/bin/%s", args[0]);
	int r;
	objid_t id;
	r = twz_name_resolve(NULL, buf, NULL, 0, &id);
	if(r) {
		r = twz_name_resolve(NULL, args[0], NULL, 0, &id);
		if(r) {
			printf("failed to resolve '%s'\n", buf);
			twz_thread_exit();
		}
	}

	r = execv(buf, args);
	fprintf(stderr, "failed to exec %s: %d", args[0], r);
	twz_thread_exit();
}

int main()
{
	char buffer[1024];

	/*
	debug_printf("Okay, trying to fork\n");
	int p = fork();
	debug_printf("Hello from: %d\n", p);

	if(!p) {
	    exit(0);
	}
	debug_printf("WAIT\n");

	int s;
	int x = wait(&s);
	debug_printf("WAITED: %d %x\n", x, s);

	for(;;)
	    ;
	    */

	char *username = getenv("USER");
	for(;;) {
		printf("%s@twz $ ", username);
		fflush(NULL);
		fgets(buffer, 1024, stdin);
		char *nl = strchr(buffer, '\n');
		if(nl)
			*nl = 0;
		if(nl == buffer)
			continue;
		if(!strcmp(buffer, "kls")) {
			kls();
			continue;
		}

		if(!fork()) {
			tmain(buffer);
		}

		int status;
		wait(&status);
	}
}
