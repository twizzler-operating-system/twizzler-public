#include "blake2.h"
#include "common.h"
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <twz/_sctx.h>
#include <unistd.h>
/*
 * mkcap -t target -a accessor -p perms -e gates -h htype -s etype -r expire kr-obj
 */

static const char *hfns[] = {
	[SCHASH_BLAKE2] = "blake2",
};

static const char *efns[] = {
	[SCENC_TEST] = "test",
	[SCENC_DSA] = "dsa",
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

	cap.slen = sigbits[cap.etype][cap.htype];

	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &cap, sizeof(cap));
	char sig[32];
	blake2b_final(&S, sig, 32);

	write(1, &cap, sizeof(cap));
	write(1, sig, 32);
	return 0;
}
