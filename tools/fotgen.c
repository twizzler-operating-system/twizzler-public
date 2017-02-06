#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>

#define FLAG_NAME 0x4
#define FLAG_PRESENT 0x2
void usage(void)
{
	fprintf(stderr, "fotgen: usage: [-o outfile] [infile]\n");
	fprintf(stderr, "options:\n  -o <outfile>: Write output to outfile.\nIf not specified, outfile is stdout and infile is stdin.\n");
}

__attribute__((packed))
struct fote {
	union {
		struct { uint64_t lo; uint64_t hi; };
		__int128 guid;
		char name[16];
	};
	uint64_t flags;
	uint64_t nr;
	uint64_t gr;
	uint64_t ad;
};

#define MAP(c, f) \
	[c] = f, \
	[c+32] = f

uint64_t fmap[128] = {
	MAP('P', FLAG_PRESENT),
	MAP('N', FLAG_NAME),
};

uint64_t parse_flags(char *str)
{
	uint64_t flags = 0;
	for(;*str;str++) {
		int c = *str;
		if(fmap[c] == 0) {
			fprintf(stderr, "unknown flag: %c\n", c);
		}
		flags |= fmap[c];
	}
	return flags;
}

int main(int argc, char **argv)
{
	int c;
	char *outpath = NULL;
	while((c=getopt(argc, argv, "o:")) != EOF) {
		switch(c) {
			case 'o':
				outpath = optarg;
				break;
			default:
				usage();
				exit(1);
		}
	}

	FILE *outfile = outpath ? fopen(outpath, "w") : stdout;
	if(!outfile) {
		perror("fopen outfile");
		exit(1);
	}

	FILE *infile = optind >= argc ? stdin : fopen(argv[optind], "r");
	if(!infile) {
		perror("fopen infile");
		exit(1);
	}

	uint64_t flags, gr, nr, ad;
	uint64_t ghi, glo;
	char *fstr;
	int entry;
	char *name;
	char *line = NULL;
	size_t llen = 0;
	while(getline(&line, &llen, infile) > 0) {
		char *cmt = strchr(line, '#');
		if(cmt) {
			*cmt = '\0';
		}
		cmt = strchr(line, '\n');
		if(cmt) {
			*cmt = '\0';
		}
		if(*line == '\0')
			continue;
		entry = strtol(strtok(line, " \t"), NULL, 0);
		fstr = strtok(NULL, " \t");
		flags = parse_flags(fstr);
		if(flags & FLAG_NAME) {
			name = strtok(NULL, " \t");
		} else {
			ghi = strtoll(strtok(NULL, ":"), NULL, 0);
			glo = strtoll(strtok(NULL, " \t"), NULL, 0);
		}
		nr = strtoll(strtok(NULL, " \t"), NULL, 0);
		gr = strtoll(strtok(NULL, " \t"), NULL, 0);
		ad = strtoll(strtok(NULL, " \t"), NULL, 0);

		if(flags & FLAG_NAME) {
			fprintf(stderr, "entry: %d: %lx %s %lx %lx %lx\n", entry, flags, name, nr, gr, ad);
		} else {
			fprintf(stderr, "entry: %d: %lx %lx:%lx %lx %lx %lx\n", entry, flags, ghi, glo, nr, gr, ad);
		}

		struct fote fe = {
			.nr = nr,
			.ad = ad,
			.gr = gr,
			.flags = flags,
		};

		if(flags & FLAG_NAME) {
			/* TODO: support string tables */
			if(strlen(name) >= 16) {
				fprintf(stderr, "I'm too lazy to write string table support, so I'm truncating your name from %s to %s. #dealwithit.\n", name, (name[15] = 0, name));
			}
			strcpy(fe.name, name);
		} else {
			fe.lo = glo;
			fe.hi = ghi;
		}

		if(fseek(outfile, sizeof(fe) * entry, SEEK_SET) == -1) {
			perror("fseek");
			exit(1);
		}

		fwrite(&fe, sizeof(fe), 1, outfile);
	}


	if(optind < argc) fclose(infile);
	if(outpath) fclose(outfile);
	return 0;
}

