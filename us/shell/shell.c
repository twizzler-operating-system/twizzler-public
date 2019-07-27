#include <stdlib.h>
#include <twz/_err.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>

const char *kso_names[] = {
	[KSO_ROOT] = "root",
	[KSO_THREAD] = "thread",
	[KSO_VIEW] = "view",
	[KSO_SECCTX] = "secctx",
};

void print_kat(struct kso_attachment *k, int indent)
{
	char name[128];
	size_t namelen = sizeof(name);
	const char *reason = "";
	int r = twz_name_reverse_lookup(k->id, name, &namelen, NULL, 0);
	if(r == -ENOSPC) {
		reason = " (name too long)";
	}

	printf("%*s", indent, "");
	if(k->type >= KSO_MAX) {
		printf("[unknown] ");
	} else {
		printf("[%s] ", kso_names[k->type]);
	}

	if(r) {
		printf(IDFMT "%s", IDPR(k->id), reason);
	} else {
		printf("%s", name);
	}
	printf("\n");
}

void kls_thread(struct kso_attachment *p, int indent)
{
	struct object thr;
	twz_object_open(&thr, p->id, FE_READ);
	struct twzthread_repr *r = twz_obj_base(&thr);
	for(int i = 0; i < TWZ_THRD_MAX_SCS; i++) {
		struct kso_attachment *k = &r->attached[i];
		if(k->id == 0 || k->type != KSO_SECCTX)
			continue;
		print_kat(k, indent);
	}
}

void kls(void)
{
	struct object root;
	twz_object_open(&root, 1, FE_READ);

	struct kso_root_repr *r = twz_obj_base(&root);
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
int main(int argc, char **argv)
{
	char buffer[1024];

	for(char **env = environ; *env != 0; env++) {
		char *thisEnv = *env;
		printf("%s\n", thisEnv);
	}

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
		}
	}
}
