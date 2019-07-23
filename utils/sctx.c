#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <twz/_sctx.h>
#include <unistd.h>
/*
 * mkcap ... | sctx ctx.obj
 */

objid_t get_target(char *data)
{
	struct sccap *cap = (void *)data;
	if(cap->magic == SC_DLG_MAGIC) {
		struct scdlg *dlg = (void *)data;
		return get_target(data + sizeof(*dlg));
	}
	return cap->target;
}

void add_hash(struct secctx *ctx, objid_t target, char *ptr)
{
	printf("adding perm object for " IDFMT " %p\n", IDPR(target), ptr);
	size_t slot = target % ctx->nbuckets;
	while(1) {
		struct scbucket *b = &ctx->buckets[slot];
		if(b->target == 0) {
			b->target = target;
			b->data = ptr;
			b->flags = 0;
			b->gatemask = (struct scgates){ 0 };
			b->pmask = ~0;
			break;
		}
		slot = b->chain;
		if(slot == 0) {
			for(size_t i = ctx->nbuckets; i < ctx->nbuckets + ctx->nchain; i++) {
				struct scbucket *n = &ctx->buckets[slot];
				if(n->chain == 0 && n->target == 0) {
					slot = i;
					break;
				}
			}
			if(slot == 0) {
				fprintf(stderr, "ctx full!\n");
				exit(1);
			}
		}
	}
}

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
	ftruncate(fd, OBJ_MAXSIZE);
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
	size_t max = sizeof(*ctx) + sizeof(struct scbucket) * (ctx->nbuckets + ctx->nchain);
	char *end = (char *)ctx + max;

	ssize_t r;
	_Alignas(16) char buffer[sizeof(struct sccap)];
	// while((r = read(0, buffer, sizeof(buffer))) > 0) {
	while(1) {
		size_t m = 0;
		while((r = read(0, buffer + m, sizeof(buffer) - m)) > 0) {
			if(r == 0 && m == 0)
				break;
			m += r;
		}
		if(r < 0) {
			err(1, "read");
		}
		if(m == 0)
			break;
		if(m != sizeof(buffer)) {
			errx(1, "incorrect read size cap (%ld / %ld)", m, sizeof(buffer));
		}
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
		m = 0;
		while((r = read(0, end + m, rem - m)) > 0) {
			m += r;
		}
		if(r < 0) {
			err(1, "read");
		}
		if(m != rem) {
			errx(1, "incorrect read size (%ld / %ld) rem", m, rem);
		}
		end += rem;

		objid_t target = get_target(ptr);
		add_hash(ctx, target, (char *)((ptr - (char *)ctx) + OBJ_NULLPAGE_SIZE));
	}
	/* TODO: truncate file to right max len */
	size_t l = end - (char *)ctx;
	munmap(ctx, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE));
	ftruncate(fd, l);
	close(fd);
	return 0;
}
