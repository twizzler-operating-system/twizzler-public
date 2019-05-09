#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

static const char *program;

static void cat(const char *name)
{
	FILE *f = strcmp("-", name) ? fopen(name, "r") : stdin;
	if(!f) {
		fprintf(stderr, "%s: %s: %s\n",
				program, name, strerror(errno));
		return;
	}

	char buf[4096];
	while(fgets(buf, sizeof(buf), f)) {
		if(fputs(buf, stdout) == EOF) {
			fprintf(stderr, "%s: stdout: %s\n",
					program, strerror(errno));
			exit(1);
		}
	}

	if(ferror(f)) {
		fprintf(stderr, "%s: %s: %s\n",
				program, name, strerror(errno));
	}
	if(f != stdin) {
		fclose(f);
	}
}

int main(int argc, char **argv)
{
	program = argv[0];
	if(argc == 1) {
		cat("-");
	} else {
		for(int i=1;i<argc;i++) {
			cat(argv[i]);
		}
	}
	return 0;
}

