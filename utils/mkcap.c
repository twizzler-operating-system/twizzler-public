#include "common.h"
#include "sign.h"
#include <err.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <twz/_sctx.h>
#include <unistd.h>

#define LTM_DESC
#include <tomcrypt.h>
/*
 * mkcap -t target -a accessor -p perms -e gates -h htype -s etype -r expire kr-pem
 */

static const char *hfns[] = {
	[SCHASH_SHA1] = "sha1",
};

static const char *efns[] = {
	[SCENC_DSA] = "dsa",
};

size_t hashlen[SCHASH_NUM] = {
	[SCHASH_SHA1] = 20,
};

int main(int argc, char **argv)
{
	int c;
	struct sccap cap = { 0 };
	cap.magic = SC_CAP_MAGIC;
	while((c = getopt(argc, argv, "t:a:p:e:h:s:r:")) != EOF) {
		switch(c) {
			char *as, *bs, *cs;
			ssize_t f;
			case 't':
				cap.target = str_to_objid(optarg);
				break;
			case 'a':
				cap.accessor = str_to_objid(optarg);
				break;
			case 'p':
				for(char *s = optarg; *s; s++) {
					switch(*s) {
						case 'r':
						case 'R':
							cap.perms |= SCP_READ;
							break;
						case 'w':
						case 'W':
							cap.perms |= SCP_WRITE;
							break;
						case 'x':
						case 'X':
							cap.perms |= SCP_EXEC;
							break;
						case 'u':
						case 'U':
							cap.perms |= SCP_USE;
							break;
						case 'd':
						case 'D':
							cap.perms |= SCP_CD;
							break;
						default:
							fprintf(stderr, "invalid perm: %c\n", *s);
							exit(1);
					}
				}
				break;
			case 'e':
				as = strtok(optarg, ",");
				bs = strtok(NULL, ",");
				cs = strtok(NULL, ",");

				if(!as || !bs || !cs) {
					fprintf(stderr, "invalid gate spec: %s\n", optarg);
					exit(1);
				}
				cap.gates.offset = strtol(as, NULL, 0);
				cap.gates.length = strtol(bs, NULL, 0);
				cap.gates.align = strtol(cs, NULL, 0);
				cap.flags |= SCF_GATE;
				break;
			case 'h':
				f = -1;
				for(size_t i = 0; i < SCHASH_NUM; i++) {
					if(!strcmp(hfns[i], optarg)) {
						f = i;
						break;
					}
				}
				if(f == -1) {
					fprintf(stderr, "unknown function %s\n", optarg);
					exit(1);
				}
				cap.htype = f;

				break;
			case 's':
				f = -1;
				for(size_t i = 0; i < SCENC_NUM; i++) {
					if(!strcmp(efns[i], optarg)) {
						f = i;
						break;
					}
				}
				if(f == -1) {
					fprintf(stderr, "unknown function %s\n", optarg);
					exit(1);
				}
				cap.etype = f;

				break;
			case 'r':
				cap.rev.create = time(NULL);
				cap.rev.valid = strtol(optarg, NULL, 0);
				cap.flags |= SCF_REV;
				break;
			default:
				fprintf(stderr, "invalid option\n");
				exit(1);
		}
	}

	char *pr = argv[optind];
	if(!pr) {
		errx(1, "no key!");
	}

	int fd = open(pr, O_RDONLY);
	if(fd == -1)
		err(1, "open");

	struct stat st;
	if(fstat(fd, &st) == -1)
		err(1, "fstat");

	char *buffer = calloc(1, st.st_size + 1);
	ssize_t r;
	size_t m = 0;
	while((r = read(fd, buffer + m, st.st_size - m)) > 0) {
		m += r;
	}
	if(r < 0) {
		err(1, "read");
	}

	char *keystart = strchr(buffer, '\n') + 1;
	char *keyend = strchr(keystart, '-');

	size_t keylen = (keyend - keystart);

	unsigned char sig[4096];
	cap.slen = 0;
	size_t siglen = 0;
	while(siglen > cap.slen || siglen == 0) {
		cap.slen = siglen;
		_Alignas(16) hash_state hs;
		sha1_init(&hs);
		sha1_process(&hs, (unsigned char *)&cap, sizeof(cap));
		unsigned char out[128];
		sha1_done(&hs, out);

		siglen = sizeof(sig);
		memset(sig, 0, siglen);
		sign_dsa(out, 20, (unsigned char *)keystart, keylen, sig, &siglen);
	}

	fprintf(stderr,
	  "constructed CAP (target=" IDFMT ", accessor=" IDFMT
	  ", perms=%x, magic=%x, ofm=%d, slen=%d)\n",
	  IDPR(cap.target),
	  IDPR(cap.accessor),
	  cap.perms,
	  cap.magic,
	  offsetof(struct sccap, magic),
	  cap.slen);
	m = 0;
	while(m < sizeof(cap)) {
		ssize_t r = write(1, &cap, sizeof(cap));
		if(r < 0)
			err(1, "write");
		m += r;
	}
	m = 0;
	while(m < cap.slen) {
		ssize_t r = write(1, sig, cap.slen);
		if(r < 0)
			err(1, "write");
		m += r;
	}
	return 0;
}
