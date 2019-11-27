#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../us/include/twz/_obj.h"

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

/*
static uint64_t octal_to_int(char *in)
{
    uint64_t res = 0;
    for(; *in; in++) {
        res <<= 3;
        res += *in - '0';
    }
    return res;
}*/

static void int_to_octal(uint64_t n, char *out)
{
	char tmp[16];
	int c = 0;
	for(; n; n >>= 3, c++) {
		tmp[c] = '0' + (n & 7);
	}
	for(c--; c >= 0; c--)
		*out++ = tmp[c];
	*out = 0;
}

static void ustar_fill(struct ustar_header *h, const char *prefix, const char *name, size_t sz)
{
	memset(h, 0, sizeof(*h));
	strncpy(h->name, name, 99);
	strncpy(h->prefix, prefix, 154);
	strcpy(h->magic, "ustar");
	strcpy(h->mode, "0644");
	strcpy(h->vers, "00");
	h->type = '0';
	int_to_octal(sz, h->size);
	memset(h->checksum, ' ', 8);
	uint32_t sum = 0;
	for(unsigned i = 0; i < sizeof(*h); i++) {
		sum += *((char *)h + i);
	}
	int_to_octal(sum, h->checksum);
}

struct list {
	char *d;
	struct list *next;
};

static void usage(void)
{
	fprintf(stderr,
	  "usage: file2obj [-z] -i input-file -o output-file -p pflags [-k kuid] [-f FOT_SPEC]...\n");
	fprintf(stderr, "-z: use zero for nonce\n");
	fprintf(
	  stderr, "valid pflags include R (dfl read), W, X, U (dfl use), H (hash data), D (delete)\n");
	fprintf(stderr, "FOT_SPEC ::= <fotentry> ':' <flags> ':' IDNAME\n");
	fprintf(stderr, "IDNAME ::= ID | NAME\n");
	fprintf(
	  stderr, "Valid fot flags include R (read), W (write), X (execute), D (derive), N (name)\n");
	fprintf(stderr, "ID ::= 128-bit identifier\n");
	fprintf(stderr, "NAME ::= string ',' pointer-to-resolver\n");
	fprintf(stderr, "example: -f 1:RWD:12348094892590384...\n");
	fprintf(stderr, "example: -f 2:RWN:foo,400000001af3\n");
}

objid_t str_to_objid(char *s)
{
	if(!s)
		return 0;
	objid_t res = 0;
	char *o = s;
	for(; *s; s++) {
		if(*s == ':')
			continue;
		if(*s == 'x')
			continue;
		if(*s == '0' && *(s + 1) == 'x')
			continue;
		res <<= 4;
		if(*s >= '0' && *s <= '9')
			res += *s - '0';
		else if(*s >= 'a' && *s <= 'f')
			res += *s - 'a' + 10;
		else if(*s >= 'A' && *s <= 'F')
			res += *s - 'A' + 10;
		else {
			fprintf(stderr, "invalid ID string: %s (%c)\n", o, *s);
			exit(1);
		}
	}
	return res;
}

void copy_data(int infd, int outfd)
{
	ssize_t r;
	char buffer[4096];
	while((r = read(infd, buffer, sizeof(buffer))) > 0) {
		if(write(outfd, buffer, r) < 0) {
			perror("write");
			exit(1);
		}
	}
	if(r < 0) {
		perror("read");
		exit(1);
	}
}

int parse_fotentry(struct fotentry *fe, char *s, char **name)
{
	char *num = strtok(s, ":");
	if(!num) {
		fprintf(stderr, "Failed to parse FOT_SPEC\n");
		exit(1);
	}
	int num_n = strtol(num, NULL, 10);

	char *flags = strtok(NULL, ":");

	if(!flags) {
		fprintf(stderr, "Failed to parse FOT_SPEC\n");
		exit(1);
	}
	uint64_t fl = 0;
	for(char *f = flags; *f; f++) {
		switch(*f) {
			case 'r':
			case 'R':
				fl |= FE_READ;
				break;
			case 'w':
			case 'W':
				fl |= FE_WRITE;
				break;
			case 'x':
			case 'X':
				fl |= FE_EXEC;
				break;
			case 'd':
			case 'D':
				fl |= FE_DERIVE;
				break;
			case 'n':
			case 'N':
				fl |= FE_NAME;
				break;
		}
	}
	fe->flags = fl;

	char *res = strtok(NULL, "");
	if(!res) {
		fprintf(stderr, "Failed to parse FOT_SPEC\n");
		exit(1);
	}

	if(fl & FE_NAME) {
		char *c = strchr(res, ',');
		if(!c) {
			fprintf(stderr, "Failed to parse FOT_SPEC: %s\n", s);
			exit(1);
		}
		*c++ = 0;
		fe->name.nresolver = (void *)strtol(c, NULL, 16);
		*name = strdup(res);
	} else {
		fe->id = str_to_objid(res);
	}
	return num_n;
}

int main(int argc, char **argv)
{
	int c;
	struct list *fotlist = NULL;
	char *outfile = NULL, *infile = NULL;
	uint16_t pflags = 0;
	char *kuidstr = NULL;
	bool zero_nonce = false;
	while((c = getopt(argc, argv, "f:o:i:k:p:z")) != EOF) {
		switch(c) {
			struct list *l;
			case 'f':
				l = (struct list *)malloc(sizeof(struct list));
				l->d = strdup(optarg);
				l->next = fotlist;
				fotlist = l;
				break;
			case 'o':
				outfile = strdup(optarg);
				break;
			case 'i':
				infile = strdup(optarg);
				break;
			case 'p':
				for(char *p = optarg; *p; p++) {
					switch(*p) {
						case 'r':
						case 'R':
							pflags |= MIP_DFL_READ;
							break;
						case 'w':
						case 'W':
							pflags |= MIP_DFL_WRITE;
							break;
						case 'x':
						case 'X':
							pflags |= MIP_DFL_EXEC;
							break;
						case 'u':
						case 'U':
							pflags |= MIP_DFL_USE;
							break;
						case 'h':
						case 'H':
							pflags |= MIP_HASHDATA;
							break;
						case 'd':
						case 'D':
							pflags |= MIP_DFL_DEL;
							break;
					}
				}
				break;
			case 'z':
				zero_nonce = true;
				break;
			case 'k':
				kuidstr = strdup(optarg);
				break;
			default:
				usage();
				return 1;
		}
	}
	if(!outfile || !infile) {
		usage();
		return 1;
	}

	int infd;
	if(!strcmp(infile, "-")) {
		infd = 0;
	} else {
		infd = open(infile, O_RDONLY);
		if(infd < 0) {
			perror("open");
			return 1;
		}
	}

	int outfd = open(outfile, O_RDWR | O_CREAT | O_TRUNC, 0644);
	if(outfd < 0) {
		perror("open out");
		return 1;
	}

	struct stat st;
	if(fstat(infd, &st) < 0) {
		perror("stat");
		return 1;
	}

	struct ustar_header header;
	ustar_fill(&header, outfile, "data", st.st_size);
	if(write(outfd, &header, sizeof(header)) < 0) {
		perror("write");
		exit(1);
	}

	copy_data(infd, outfd);

	off_t off = lseek(outfd, 0, SEEK_CUR);
	if(off < 0) {
		perror("lseek");
		exit(1);
	}
	off = ((off - 1) & ~511) + 512;
	if(lseek(outfd, off, SEEK_SET) < 0) {
		perror("lseek");
		exit(1);
	}

	int max_fote = 0;
	size_t nameln = 0;
	for(struct list *l = fotlist; l; l = l->next) {
		struct fotentry fe = { 0 };
		char *name;
		int e = parse_fotentry(&fe, strdup(l->d), &name);
		if(e > max_fote)
			max_fote = e;
		if(name)
			nameln += strlen(name) + 1;
	}
	size_t metaext_size = 128;
	size_t ussz = OBJ_METAPAGE_SIZE + sizeof(struct metainfo) + metaext_size + nameln;
	// sizeof(struct fotentry) * (max_fote + 1) + nameln + sizeof(struct metainfo) + metaext_size;
	ustar_fill(&header, outfile, "meta", ussz);
	if(write(outfd, &header, sizeof(header)) < 0) {
		perror("write");
		exit(1);
	}

	objid_t kuid = str_to_objid(kuidstr);

	struct metainfo mi = {
		.magic = MI_MAGIC,
		.sz = st.st_size,
		.flags = MIF_SZ,
		.p_flags = pflags,
		.milen = sizeof(mi) + metaext_size,
		.fotentries = max_fote + 1,
		.kuid = kuid,
	};
	if(zero_nonce) {
		memset(&mi.nonce, 0, sizeof(mi.nonce));
	} else {
		if(getrandom(&mi.nonce, sizeof(mi.nonce), 0) < 0) {
			perror("getrandom");
			return 1;
		}
	}

	off_t metapage_start = lseek(outfd, OBJ_METAPAGE_SIZE, SEEK_CUR);
	if(write(outfd, &mi, sizeof(mi)) < 0) {
		perror("write");
		exit(1);
	}

	off_t fotstart = metapage_start;
	off_t namestart = metapage_start + metaext_size + sizeof(mi);
	for(struct list *l = fotlist; l; l = l->next) {
		struct fotentry fe = { 0 };
		char *name = NULL;
		unsigned int e = parse_fotentry(&fe, l->d, &name);
		if(e >= OBJ_METAPAGE_SIZE / sizeof(struct fotentry)) {
			fprintf(stderr, "out of FOT entries\n");
			return 1;
		}
		if(name) {
			if((strlen(name) + 1 + namestart) - metapage_start >= OBJ_METAPAGE_SIZE) {
				fprintf(stderr, "Out of space for names\n");
				return 1;
			}
			if(pwrite(outfd, name, strlen(name) + 1, namestart) < 0) {
				perror("pwrite");
				return 1;
			}
			fe.name.data = (char *)(OBJ_MAXSIZE - OBJ_METAPAGE_SIZE + sizeof(mi) + namestart);
			namestart += strlen(name) + 1;
		}
		if(pwrite(outfd, &fe, sizeof(fe), fotstart - e * sizeof(fe)) < 0) {
			perror("pwrite");
			return 1;
		}
	}
	pwrite(outfd, "", 1, metapage_start + 0x1000 - 1);
	return 0;
}
