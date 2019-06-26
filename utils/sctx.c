#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <twz/_sctx.h>
#include <unistd.h>
/*
 * mkcap ... | sctx ctx.obj
 */

int main(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "h")) != EOF) {
		switch(c) {
			default:
				fprintf(stderr, "invalid option %c\n", c);
				exit(1);
		}
	}
	char *outname = argv[optind];
	int fd = open(outname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if(fd == -1) {
		perror("open");
		exit(1);
	}
	struct secctx *ctx = mmap(NULL,
	  OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE),
	  PROT_READ | PROT_WRITE,
	  MAP_SHARED,
	  fd,
	  0);
	if(ctx == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}

	ctx->nbuckets = 1024;
	ctx->nchain = 4096;
	size_t max = sizeof(ctx) + sizeof(struct scbucket) * (ctx->nbuckets + ctx->nchain);
	char *end = (char *)ctx + max;

	ssize_t r;
	_Alignas(16) char buffer[sizeof(struct sccap)];
	while(1) {
		r = read(0, buffer, sizeof(buffer));
		struct sccap *cap = (void *)buffer;
		struct scdlg *dlg = (void *)buffer;
		size_t rem;
		switch(cap->magic) {
			case SC_CAP_MAGIC:
				rem = cap->slen;
				break;
			case SC_DLG_MAGIC:
				rem = dlg->slen + dlg->dlen;
				break;
			default:
				fprintf(stderr, "unknown cap or dlg magic!\n");
				exit(1);
		}
		char *ptr = end;
		memcpy(end, buffer, sizeof(buffer));
		end += sizeof(buffer);
		if(read(1, end, rem) != (ssize_t)rem) {
			perror("read");
			exit(1);
		}
		end += rem;
	}

	return 0;
}
