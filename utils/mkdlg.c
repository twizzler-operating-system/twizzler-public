#include "blake2.h"
#include "common.h"
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <twz/_sctx.h>
#include <unistd.h>

/*
 * mkcap [-d] -a delegator -t delegatee -m mask -e gatemask -h htype -s etype -r expire kr-obj
 */
static const char *hfns[] = {
	[SCHASH_BLAKE2] = "blake2",
};

static const char *efns[] = {
	[SCENC_TEST] = "test",
	[SCENC_RSA] = "rsa",
};

#define BITS_PER_BYTE 8
size_t sigbits[SCENC_NUM][SCHASH_NUM] = {
	[SCENC_TEST] = {
		[SCHASH_BLAKE2] = 256 / BITS_PER_BYTE,
	},
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
				dlg.flags |= SCF_TYPE_DLG;
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

	dlg.slen = sigbits[dlg.etype][dlg.htype];

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

	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &dlg, sizeof(dlg));
	blake2b_update(&S, buffer, count);
	char sig[32];
	blake2b_final(&S, sig, 32);

	write(1, &dlg, sizeof(dlg));
	write(1, buffer, count);
	write(1, sig, 32);
	return 0;
}
