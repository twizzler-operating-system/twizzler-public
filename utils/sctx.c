#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <twz/_sctx.h>
#include <unistd.h>

#include "common.h"

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

void add_hash(struct secctx *ctx, objid_t target, char *ptr, uint32_t mask, uint64_t flags)
{
	// fprintf(stderr, "adding perm object for " IDFMT " %p\n", IDPR(target), ptr);
	size_t slot = target % ctx->nbuckets;
	while(1) {
		struct scbucket *b = &ctx->buckets[slot];
		if(b->target == 0) {
			b->target = target;
			b->data = ptr;
			b->flags = flags;
			b->gatemask = (struct scgates){ 0 };
			b->pmask = mask;
			break;
		}
		slot = b->chain;
		if(slot == 0) {
			for(size_t i = ctx->nbuckets; i < ctx->nbuckets + ctx->nchain; i++) {
				struct scbucket *n = &ctx->buckets[slot];
				if(n->chain == 0 && n->target == 0) {
					b->chain = slot = i;
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

static uint32_t parse_mask(char *s)
{
	uint32_t m = 0;
	while(*s) {
		switch(*s) {
			case 'r':
			case 'R':
				m |= SCP_READ;
				break;
			case 'w':
			case 'W':
				m |= SCP_WRITE;
				break;
			case 'x':
			case 'X':
				m |= SCP_EXEC;
				break;
			case 'u':
			case 'U':
				m |= SCP_USE;
				break;
			case 'd':
			case 'D':
				m |= SCP_DEL;
				break;
			default:
				errx(1, "invalid mask character %c\n", *s);
		}
		s++;
	}
	return m;
}

struct ad {
	struct ad *next;
	char *data;
};

int main(int argc, char **argv)
{
	int c;
	char *name = NULL;
	uint32_t gmask = 0;

	struct ad *ad = NULL;
	while((c = getopt(argc, argv, "hn:a:")) != EOF) {
		switch(c) {
			case 'n':
				name = optarg;
				break;
			case 'g':
				gmask = parse_mask(optarg);
				break;
			case 'a': {
				struct ad *a = malloc(sizeof(*a));
				a->data = strdup(optarg);
				a->next = ad;
				ad = a;
			} break;
			default:
				fprintf(stderr, "invalid option %c\n", c);
				exit(1);
		}
	}
	char *outname = argv[optind];
	int fd = open(outname, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if(fd == -1) {
		err(1, "open: %s\n", outname);
	}
	ftruncate(fd, OBJ_MAXSIZE);
	struct secctx *ctx = mmap(NULL,
	  OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE),
	  PROT_READ | PROT_WRITE,
	  MAP_SHARED,
	  fd,
	  0);
	if(name)
		strncpy(ctx->hdr.name, name, KSO_NAME_MAXLEN - 1);
	if(ctx == MAP_FAILED) {
		perror("mmap");
		ftruncate(fd, 0);
		exit(1);
	}

	ctx->nbuckets = 1024;
	ctx->nchain = 4096;
	ctx->gmask = gmask;
	size_t max = sizeof(*ctx) + sizeof(struct scbucket) * (ctx->nbuckets + ctx->nchain);
	max = ((max - 1) & ~15) + 16;
	char *end = (char *)ctx + max;

	ssize_t r;
	_Alignas(16) char buffer[sizeof(struct sccap)];
	// while((r = read(0, buffer, sizeof(buffer))) > 0) {
	while(1) {
		size_t m = 0;
		while((sizeof(buffer) - m > 0) && (r = read(0, buffer + m, sizeof(buffer) - m)) > 0) {
			if(r == 0 && m == 0)
				break;
			m += r;
		}
		if(r < 0) {
			ftruncate(fd, 0);
			err(1, "read");
		}
		if(m == 0)
			break;
		if(m != sizeof(buffer)) {
			ftruncate(fd, 0);
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
				fprintf(stderr, "unknown cap or dlg magic! (%x)\n", cap->magic);
				fprintf(stderr, "Off: %ld\n", offsetof(struct sccap, magic));
				for(unsigned int i = 0; i < sizeof(buffer); i++) {
					if(i % 16 == 0)
						fprintf(stderr, "\n");
					fprintf(stderr, "%2x ", (unsigned char)buffer[i]);
				}
				ftruncate(fd, 0);
				exit(1);
		}
		char *ptr = end;
		memcpy(end, buffer, sizeof(buffer));
		end += sizeof(buffer);
		m = 0;
		while((rem - m > 0) && (r = read(0, end + m, rem - m)) > 0) {
			m += r;
		}
		if(r < 0) {
			ftruncate(fd, 0);
			err(1, "read");
		}
		if(m != rem) {
			ftruncate(fd, 0);
			errx(1, "incorrect read size (%ld / %ld) rem", m, rem);
		}
		size_t inc = ((rem - 1) & ~15) + 16;
		end += inc;

		objid_t target = get_target(ptr);
		add_hash(ctx, target, (char *)((ptr - (char *)ctx) + OBJ_NULLPAGE_SIZE), ~0, 0);
	}

	for(struct ad *a = ad; a; a = a->next) {
		char type = a->data[0];
		char *ids = a->data + 2;
		char *mask = strchr(ids, ':');
		*mask = 0;
		mask++;

		objid_t id = str_to_objid(ids);
		uint32_t m = parse_mask(mask);

		add_hash(ctx, id, NULL, ~m, type == 'r' ? SCF_TYPE_REGRANT_MASK : 0);
	}

	/* TODO: truncate file to right max len */
	size_t l = end - (char *)ctx;
	munmap(ctx, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE));
	ftruncate(fd, l);
	close(fd);
	return 0;
}
