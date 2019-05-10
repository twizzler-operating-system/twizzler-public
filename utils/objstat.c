#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../us/include/twz/_obj.h"
#include "blake2.h"

bool show_only_id = false;

struct ustar_header {
	char name[100];
	char mode[8];
	char uid[8];
	char gid[8];
	char size[12];
	char mtime[12];
	char checksum[8];
	char type;
	char nlf[100];
	char magic[6];
	char vers[2];
	char ownname[32];
	char owngroup[32];
	char devmaj[8];
	char devmin[8];
	char prefix[155];
	char pad[12];
};

_Static_assert(sizeof(struct ustar_header) == 512, "USTAR header not 512 bytes!");

static uint64_t octal_to_int(char *in)
{
	uint64_t res = 0;
	for(; *in; in++) {
		res <<= 3;
		res += *in - '0';
	}
	return res;
}

objid_t compute_objid(char *base)
{
	/* b = blake2(obj->nonce ++ obj->kuid? ++ data? ++ idmode) *
	 * id = (b >> 128) | b */
	struct metainfo *mi = (struct metainfo *)(base + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE);
	unsigned char tmp[32];
	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &mi->nonce, sizeof(mi->nonce));
	blake2b_update(&S, &mi->p_flags, sizeof(mi->p_flags));
	blake2b_update(&S, &mi->kuid, sizeof(mi->kuid));
	if(mi->p_flags & MIP_HASHDATA) {
		blake2b_update(&S, base, mi->sz);
		blake2b_update(&S, base + OBJ_MAXSIZE - mi->mdbottom, mi->mdbottom);
	}
	blake2b_final(&S, tmp, 32);
	unsigned char out[16];
	for(int i = 0; i < 16; i++) {
		out[i] = tmp[i] ^ tmp[i + 16];
	}
	return *(objid_t *)out;
}

void objstat(char *path)
{
	int fd = open(path, O_RDONLY);
	if(fd < 0) {
		perror("open");
		exit(1);
	}
	char *m =
	  (char *)mmap(NULL, OBJ_MAXSIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if(m == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	struct ustar_header h;
	if(read(fd, &h, sizeof(h)) != sizeof(h)) {
		perror("read");
		exit(1);
	}

	ssize_t sz = octal_to_int(h.size);
	if(read(fd, m + OBJ_NULLPAGE_SIZE, sz) != sz) {
		perror("read");
		exit(1);
	}

	off_t off = lseek(fd, 0, SEEK_CUR);
	off = ((off - 1) & ~511) + 512;
	lseek(fd, off, SEEK_SET);

	if(read(fd, &h, sizeof(h)) != sizeof(h)) {
		perror("read");
		exit(1);
	}
	sz = octal_to_int(h.size);
	if(sz != OBJ_METAPAGE_SIZE) {
		fprintf(stderr, "UNSUP: meta more than 0x1000\n");
		exit(1);
	}
	if(read(fd, m + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE, sz) != sz) {
		perror("read");
		exit(1);
	}

	struct metainfo *mi = (struct metainfo *)(m + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE);
	if(mi->magic != MI_MAGIC) {
		printf("Invalid object (magic = %x)\n", mi->magic);
		exit(1);
	}
	objid_t id = compute_objid(m);
	if(!show_only_id) {
		printf("METAINFO\n");
		printf("     FIELD VALUE\n");
		printf("     magic %s\n", mi->magic == MI_MAGIC ? "ok" : "bad");
		printf("     flags %x\n", mi->flags);
		printf("   p_flags %x\n", mi->p_flags);
		printf("     milen %d\n", mi->milen);
		printf("fotentries %d\n", mi->fotentries);
		printf("  mdbottom %x\n", mi->mdbottom);
		printf("        sz %ld\n", mi->sz);
		printf("     nonce " IDFMT "\n", IDPR(mi->nonce));
		printf("      kuid " IDFMT "\n", IDPR(mi->kuid));

		if(mi->fotentries) {
			struct fotentry *fe =
			  (struct fotentry *)(m + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE + mi->milen);
			printf("FOT ENTRIES\n");
			printf("    # FLAGS ID/NAME\n");
			for(unsigned i = 0; i < mi->fotentries; i++, fe++) {
				char fl[6];
				char *f = fl;
				if(fe->flags & FE_READ)
					*f++ = 'R';
				if(fe->flags & FE_WRITE)
					*f++ = 'W';
				if(fe->flags & FE_EXEC)
					*f++ = 'X';
				if(fe->flags & FE_DERIVE)
					*f++ = 'D';
				if(fe->flags & FE_NAME)
					*f++ = 'N';
				*f = 0;
				printf("%5d %5s ", i, fl);
				if(fe->flags & FE_NAME) {
					if((uintptr_t)fe->name.data > OBJ_MAXSIZE) {
						printf("foreign name pointer: %p", fe->name.data);
					} else {
						printf("%s", m + (uintptr_t)fe->name.data);
					}
					printf("!%p", fe->name.nresolver);
				} else {
					printf(IDFMT, IDPR(fe->id));
				}
				printf("\n");
			}
		}
		printf("COMPUTED OBJECT ID: " IDFMT "\n", IDPR(id));
	} else {
		printf(IDFMT "\n", IDPR(id));
	}
}

int main(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "i")) != EOF) {
		switch(c) {
			case 'i':
				show_only_id = true;
				break;
			default:
				fprintf(stderr, "usage: objstat [-i] file\n");
				exit(1);
		}
	}
	if(optind >= argc) {
		fprintf(stderr, "usage: objstat [-i] file\n");
		exit(1);
	}
	objstat(argv[optind]);
	return 0;
}
