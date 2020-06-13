#define _GNU_SOURCE

#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <twz/_key.h>
#include <twz/_sctx.h>
#include <unistd.h>

void usage(void)
{
	fprintf(stderr, "makekey -i private-key-pem-in -r privkey-out -u pubkey-out -t type\n");
	fprintf(stderr, "valid types: dsa\n");
}

int main(int argc, char **argv)
{
	int c;
	char *infile = NULL;
	char *pr = NULL;
	char *pu = NULL;
	char *type = NULL;
	while((c = getopt(argc, argv, "r:u:i:t:")) != EOF) {
		switch(c) {
			case 'r':
				pr = optarg;
				break;
			case 'u':
				pu = optarg;
				break;
			case 'i':
				infile = optarg;
				break;
			case 't':
				type = optarg;
				break;
			default:
				usage();
				exit(1);
		}
	}

	if(!infile || !pu || !pr || !type) {
		usage();
		exit(1);
	}

	int t;
	if(!strcmp(type, "dsa")) {
		t = SCENC_DSA;
	} else {
		usage();
		exit(1);
	}

	char buffer[strlen(infile) + 1024];
	sprintf(buffer, "openssl dsa -in %s -pubout 2>/dev/null", infile);
	FILE *f = popen(buffer, "r");
	if(!f) {
		err(1, "popen");
	}
	/* should be enough... */
	size_t max = 0x1000;
	char *pubdata = malloc(max);
	size_t dp = 0;
	ssize_t thisread;
	while((thisread = read(fileno(f), pubdata + dp, max - dp)) > 0) {
		dp += thisread;
		if(dp >= max)
			abort();
	}

	if(thisread < 0) {
		err(1, "read");
	}

	int r = pclose(f);
	if(r != 0) {
		fprintf(stderr, "pcommand returned %d\n", r);
		exit(r);
	}

	struct key_hdr h = {
		.keydata = (unsigned char *)((long)OBJ_NULLPAGE_SIZE + sizeof(struct key_hdr)),
		.keydatalen = dp,
		.flags = 0,
		.type = t,
	};
	int fd = open(pu, O_CREAT | O_TRUNC | O_RDWR, 0644);
	if(fd == -1) {
		err(1, "open: %s", pu);
	}

	if(write(fd, &h, sizeof(h)) != sizeof(h))
		err(1, "write");
	if(write(fd, pubdata, dp) != (ssize_t)dp)
		err(1, "write");
	close(fd);

	fd = open(pr, O_CREAT | O_TRUNC | O_RDWR, 0644);
	if(fd == -1) {
		err(1, "open: %s", pr);
	}
	int prfd = open(infile, O_RDONLY);
	if(prfd == -1) {
		err(1, "open");
	}

	h = (struct key_hdr){
		.keydata = (unsigned char *)((long)OBJ_NULLPAGE_SIZE + sizeof(struct key_hdr)),
		.keydatalen = dp,
		.flags = TWZ_KEY_PRI,
		.type = t,
	};

	if(write(fd, &h, sizeof(h)) != sizeof(h))
		err(1, "write");
	struct stat st;
	if(fstat(prfd, &st) == -1)
		err(1, "fstat");
	off_t l = sizeof(h);
	if(copy_file_range(prfd, NULL, fd, &l, st.st_size, 0) != st.st_size)
		err(1, "copy_file_range");
	close(fd);

	return 0;
}
