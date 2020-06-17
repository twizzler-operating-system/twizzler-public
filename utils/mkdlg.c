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
 * mkcap [-d] -a delegator -t delegatee -m mask -e gatemask -h htype -s etype -r expire kr-obj
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
	struct scdlg dlg = { 0 };
	dlg.magic = SC_DLG_MAGIC;
	while((c = getopt(argc, argv, "dt:a:m:e:h:s:r:")) != EOF) {
		switch(c) {
			char *as, *bs, *cs;
			ssize_t f;
			case 'd':
				//			dlg.flags |= SCF_TYPE_DLG;
				break;
			case 't':
				dlg.delegatee = str_to_objid(optarg);
				break;
			case 'a':
				dlg.delegator = str_to_objid(optarg);
				break;
			case 'm':
				for(char *s = optarg; *s; s++) {
					switch(*s) {
						case 'r':
						case 'R':
							dlg.mask |= SCP_READ;
							break;
						case 'w':
						case 'W':
							dlg.mask |= SCP_WRITE;
							break;
						case 'x':
						case 'X':
							dlg.mask |= SCP_EXEC;
							break;
						case 'u':
						case 'U':
							dlg.mask |= SCP_USE;
							break;
						case 'd':
						case 'D':
							dlg.mask |= SCP_CD;
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
				dlg.gates.offset = strtol(as, NULL, 0);
				dlg.gates.length = strtol(bs, NULL, 0);
				dlg.gates.align = strtol(cs, NULL, 0);
				dlg.flags |= SCF_GATE;
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
				dlg.htype = f;

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
				dlg.etype = f;

				break;
			case 'r':
				dlg.rev.create = time(NULL);
				dlg.rev.valid = strtol(optarg, NULL, 0);
				dlg.flags |= SCF_REV;
				break;
			default:
				fprintf(stderr, "invalid option\n");
				exit(1);
		}
	}

	size_t bsz = 1024, count = 0;
	char *buffer = malloc(bsz);
	ssize_t r;
	while((r = read(0, buffer + count, bsz - count)) > 0) {
		count += r;
		if(count >= bsz) {
			bsz <<= 2;
			buffer = realloc(buffer, bsz);
		}
	}

	dlg.dlen = count;

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

	char *fdata = calloc(1, st.st_size + 1);
	size_t m = 0;
	while((r = read(fd, fdata + m, st.st_size - m)) > 0) {
		m += r;
	}
	if(r < 0) {
		err(1, "read");
	}

	char *keystart = strchr(fdata, '\n') + 1;
	char *keyend = strchr(keystart, '-');

	size_t keylen = (keyend - keystart);

	unsigned char sig[4096];
	dlg.slen = 0;
	size_t siglen = 0;
	while(siglen != dlg.slen || siglen == 0) {
		dlg.slen = siglen;
		_Alignas(16) hash_state hs;
		sha1_init(&hs);
		sha1_process(&hs, (unsigned char *)&dlg, sizeof(dlg));
		sha1_process(&hs, (unsigned char *)buffer, count);
		unsigned char out[128];
		sha1_done(&hs, out);

		siglen = sizeof(sig);
		memset(sig, 0, siglen);
		sign_dsa(out, 20, (unsigned char *)keystart, keylen, sig, &siglen);
	}

	write(1, &dlg, sizeof(dlg));
	write(1, buffer, count);
	write(1, sig, dlg.slen);
	return 0;
}
