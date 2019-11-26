#include <setjmp.h>
#include <stdlib.h>
#include <twz/_err.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/fault.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>

static jmp_buf buf;

static bool human_readable = false;

void __nls_fault_handler(int f, void *info)
{
	(void)f;
	(void)info;
	longjmp(buf, 1);
}

void print_size(size_t sz)
{
	const char *str = "";
	const char *possibles[] = {
		"KB",
		"MB",
		"GB",
		"TB",
	};
	for(size_t i = 0; i < sizeof(possibles) / sizeof(possibles[0]) && sz > 1024; i++) {
		str = possibles[i];
		sz /= 1024;
	}
	if(*str)
		printf("%7ld %s ", sz, str);
	else
		printf("%10ld ", sz);
}

void nls_print(struct twz_nament *ne, bool info, bool read, bool id)
{
	if(!info) {
		if(id) {
			printf(IDFMT " %s\n", IDPR(ne->id), ne->name);
		} else {
			printf("%s\n", ne->name);
		}
	} else {
		if(read) {
			twzobj obj;
			twz_object_init_guid(&obj, ne->id, FE_READ);

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
					if(human_readable)
						print_size(mi->sz);
					else
						printf("%10ld ", mi->sz);
				} else {
					printf("         * ");
				}
			}
		} else {
			printf("!------               ?          ? ");
		}
		if(id) {
			printf(IDFMT " %s\n", IDPR(ne->id), ne->name);
		} else {
			printf("%s\n", ne->name);
		}
	}
}

void nls(bool id, bool l)
{
	static _Alignas(_Alignof(struct twz_nament)) char buffer[1024];
	twz_fault_set(FAULT_SCTX, __nls_fault_handler);

	char *startname = NULL;
	ssize_t r;
	static size_t i;
	static struct twz_nament *ne;
	if(l) {
		if(id)
			printf(" PFLAGS         PUB KEY       SIZE                                ID NAME\n");
		else
			printf(" PFLAGS         PUB KEY       SIZE NAME\n");
	}
	while((r = twz_name_dfl_getnames(startname, (void *)buffer, sizeof(buffer))) > 0) {
		ne = (void *)buffer;
		for(i = 0; i < (size_t)r; ne = (void *)((uintptr_t)ne + ne->reclen), i++) {
			if(!setjmp(buf)) {
				nls_print(ne, l, true, id);
			} else {
				nls_print(ne, l, false, id);
			}
			startname = ne->name;
		}
	}

	if(r < 0) {
		fprintf(stderr, "err: %ld\n", r);
	}
}

void usage(void)
{
	fprintf(stderr, "nls [-li]: Print Twizzler Default Namespace\n");
	fprintf(stderr, "-l: Show information about objects\n");
	fprintf(stderr, "-i: Show object ID\n");
	exit(0);
}

int main(int argc, char **argv)
{
	bool l = false;
	bool id = false;
	int c;
	while((c = getopt(argc, argv, "lih")) != EOF) {
		switch(c) {
			case 'l':
				l = true;
				break;
			case 'i':
				id = true;
				break;
			case 'h':
				human_readable = true;
				break;
			default:
				usage();
		}
	}
	nls(id, l);
	return 0;
}
