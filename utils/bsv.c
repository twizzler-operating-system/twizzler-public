#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../us/include/twz/_obj.h"
#include "../us/include/twz/_view.h"

static unsigned __int128 getid(const char *s)
{
	uint64_t hi, lo;
	sscanf(s, "%lx:%lx", &hi, &lo);
	return (((unsigned __int128)hi) << 64) | lo;
}

int main(int argc, char **argv)
{
	char *out = argv[1];
	FILE *outf = fopen(out, "w+");

	struct viewentry *ve = malloc(sizeof(struct viewentry) * 0x20000);
	memset(ve, 0, sizeof(*ve) * 0x20000);
	for(int i = 2; i < argc; i++) {
		char *sl = strdup(argv[i]);
		char *id = strchr(sl, ',');
		if(!id) {
			fprintf(stderr, "malformed directive: %s\n", argv[i]);
		}
		*id++ = 0;
		char *pr = strchr(id, ',');
		if(!pr) {
			fprintf(stderr, "malformed directive: %s\n", argv[i]);
		}
		*pr++ = 0;
		size_t num_slot = strtol(sl, NULL, 0);
		unsigned __int128 num_id = getid(id);
		uint64_t flags = VE_VALID;
		while(*pr) {
			switch(*pr) {
				case 'r':
				case 'R':
					flags |= VE_READ;
					break;
				case 'w':
				case 'W':
					flags |= VE_WRITE;
					break;
				case 'x':
				case 'X':
					flags |= VE_EXEC;
					break;
			}
			pr++;
		}
#if 0
		printf("Adding entry: %ld -> %16.16lx:%16.16lx (%lx)\n",
		  num_slot,
		  (uint64_t)(num_id >> 64),
		  (uint64_t)num_id,
		  flags);
#endif
		ve[num_slot] = (struct viewentry){ .id = num_id, .flags = flags, .res0 = 0, .res1 = 0 };
	}
	fwrite(ve, sizeof(struct viewentry), 0x20000, outf);
	fclose(outf);
}
