#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../us/include/twz/_obj.h"
#include "blake2.h"

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

static uint64_t octal_to_int(char *in)
{
	uint64_t res = 0;
	for(; *in; in++) {
		res <<= 3;
		res += *in - '0';
	}
	return res;
}

void appendobj(const char *path)
{
	int fd = open(path, O_RDWR);
	if(fd == -1) {
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
		perror("read1");
		exit(1);
	}

	size_t datalen = octal_to_int(h.size);
	ssize_t r;
	if((r = read(fd, m + OBJ_NULLPAGE_SIZE, datalen)) != (ssize_t)datalen) {
		perror("read2");
		exit(1);
	}

	off_t off = lseek(fd, 0, SEEK_CUR);
	off = ((off - 1) & ~511) + 512;
	lseek(fd, off, SEEK_SET);

	struct ustar_header mh;
	if(read(fd, &mh, sizeof(mh)) != sizeof(mh)) {
		perror("read3");
		exit(1);
	}

	size_t metalen = octal_to_int(mh.size);
	if(metalen > 0x1000) {
		perror("UNSUP: large meta");
		exit(1);
	}
	if(read(fd, m + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE, metalen) != (ssize_t)metalen) {
		perror("read4");
		exit(1);
	}

	struct metainfo *mi = (struct metainfo *)(m + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE);
	if(mi->magic != MI_MAGIC) {
		fprintf(stderr, "Invalid object (magic = %x)\n", mi->magic);
		exit(1);
	}

	if(ftruncate(fd, 0) == -1) {
		perror("ftruncate");
		exit(1);
	}
	/* leave room for the header (which we can't write yet because we don't know new len) */
	lseek(fd, 512, SEEK_SET);

	size_t count = 0;
	while(count < datalen) {
		r = write(fd, m + OBJ_NULLPAGE_SIZE + count, datalen - count);
		if(r < 0) {
			perror("write");
			exit(1);
		}
		count += r;
	}

	char buffer[4096];
	size_t newlen = 0;
	while((r = read(0, buffer, sizeof(buffer))) > 0) {
		if(write(fd, buffer, r) != r) {
			perror("write2");
			exit(1);
		}
		newlen += r;
	}
	if(r < 0) {
		perror("read5");
		exit(1);
	}

	off = lseek(fd, 0, SEEK_CUR);
	off = ((off - 1) & ~511) + 512;
	lseek(fd, off, SEEK_SET);

	mi->sz = newlen + datalen;
	if(write(fd, &mh, sizeof(mh)) != sizeof(mh)) {
		perror("writemh");
		exit(1);
	}

	if(write(fd, m + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE, metalen) != (ssize_t)metalen) {
		perror("writeme");
		exit(1);
	}

	lseek(fd, 0, SEEK_SET);

	int_to_octal(datalen + newlen, h.size);
	if(write(fd, &h, sizeof(h)) != sizeof(h)) {
		perror("writeh");
		exit(1);
	}

	close(fd);
}

int main(int argc, char **argv)
{
	int c;
	while((c = getopt(argc, argv, "h")) != EOF) {
		switch(c) {
			case 'h':
			default:
				fprintf(stderr,
				  "usage: appendobj obj\nCopies stdin to object data, updating necessary metadata "
				  "for the object.");
				exit(1);
		}
	}
	if(optind >= argc) {
		fprintf(stderr, "usage: objstat [-i] file\n");
		exit(1);
	}
	appendobj(argv[optind]);
	return 0;
}
