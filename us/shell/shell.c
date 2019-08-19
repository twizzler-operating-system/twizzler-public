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
#include <setjmp.h>

#include <twz/fault.h>
static jmp_buf buf;

void __nls_fault_handler(int f, struct fault_sctx_info *info)
{
	longjmp(buf, 1);
}

void nls_print(struct twz_nament *ne, bool info, bool read)
{
	if(!info) {
		printf("%s\n", ne->name);
	} else {
		if(read) {
			struct object obj;
			twz_object_open(&obj, ne->id, FE_READ);

			fflush(stdout);
			struct metainfo *mi = twz_object_meta(&obj);

			if(mi->magic != MI_MAGIC) {
				printf("?------               ?          ? ");
			} else {
				char buf[16];
				char *bp = buf;
				if(mi->p_flags & MIP_HASHDATA)
					*bp++ = 'h';
				else
					*bp++ = '-';
				if(mi->p_flags & MIP_DFL_READ)
					*bp++ = 'r';
				else
					*bp++ = '-';
				if(mi->p_flags & MIP_DFL_WRITE)
					*bp++ = 'w';
				else
					*bp++ = '-';
				if(mi->p_flags & MIP_DFL_EXEC)
					*bp++ = 'x';
				else
					*bp++ = '-';
				if(mi->p_flags & MIP_DFL_USE)
					*bp++ = 'u';
				else
					*bp++ = '-';
				if(mi->p_flags & MIP_DFL_DEL)
					*bp++ = 'd';
				else
					*bp++ = '-';
				*bp = 0;

				printf(" %s ", buf);
				if(mi->kuid) {
					char kuname[128];
					size_t kunamelen = sizeof(kuname);
					int r = twz_name_reverse_lookup(mi->kuid, kuname, &kunamelen, NULL, 0);

					const char *reason = "";
					if(r == -ENOENT) {
						reason = "(unknown kuid)";
					} else if(r == -ENOSPC) {
						reason = "(name too long)";
					}
					printf("%15s ", r ? reason : kuname);
				} else {
					printf("%15s ", "*");
				}
				if(mi->flags & MIF_SZ) {
					printf("%10ld ", mi->sz);
				} else {
					printf("         * ");
				}
			}
		} else {
			printf("!------               ?          ? ");
		}
		printf(IDFMT " %-40s\n", IDPR(ne->id), ne->name);
	}
}

void nls(void)
{
	static _Alignas(_Alignof(struct twz_nament)) char buffer[1024];
	twz_fault_set(FAULT_SCTX, __nls_fault_handler);

	char *startname = NULL;
	ssize_t r;
	static size_t i;
	static struct twz_nament *ne;
	printf(" PFLAGS         PUB KEY       SIZE                                ID NAME\n");
	while((r = twz_name_dfl_getnames(startname, (void *)buffer, sizeof(buffer))) > 0) {
		ne = (void *)buffer;
		for(i = 0; i < (size_t)r; ne = (void *)((uintptr_t)ne + ne->reclen), i++) {
			if(!setjmp(buf)) {
				nls_print(ne, true, true);
			} else {
				nls_print(ne, true, false);
			}
			startname = ne->name;
		}
	}

	if(r < 0) {
		fprintf(stderr, "err: %ld\n", r);
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
		if(!strcmp(buffer, "nls")) {
			nls();
		}
	}
}
