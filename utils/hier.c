#include <assert.h>
#include <err.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <twz/hier.h>
#include <unistd.h>

#include "common.h"

int main(int argc, char **argv)
{
	int c;
	int append = 0;
	FILE *f = stdout;
	while((c = getopt(argc, argv, "a:A")) != -1) {
		switch(c) {
			case 'a':
				append = 1;
				f = fopen(optarg, "a+");
				if(!f) {
					err(1, "failed to open %s for appending", optarg);
				}
				break;
			case 'A':
				append = 1;
				break;
			default:
				errx(1, "unknown option %c\n", c);
		}
	}
	size_t ln = 0;
	char *buffer;
	if(!append) {
		struct twz_namespace_hdr hdr = { .magic = NAMESPACE_MAGIC };
		fwrite(&hdr, sizeof(hdr), 1, stdout);
	} else {
		if(f != stdout)
			fseek(f, 0, SEEK_END);
	}
	while(getline(&buffer, &ln, stdin) != -1) {
		/* type objid name */
		int type;
		char *typestr = strtok(buffer, " ");
		assert(typestr != NULL);

		bool sym = false;
		switch(typestr[0]) {
			case 'r':
				type = NAME_ENT_REGULAR;
				break;
			case 'd':
			case 'n':
				type = NAME_ENT_NAMESPACE;
				break;
			case 's':
				sym = true;
				type = NAME_ENT_SYMLINK;
				break;
			default:
				errx(1, "unknown type %c\n", typestr[0]);
		}

		char *idstr = strtok(NULL, " ");
		assert(idstr != NULL);
		objid_t id = 0;
		char *symstr = NULL;
		if(sym) {
			symstr = idstr;
		} else {
			id = str_to_objid(idstr);
		}

		char *name = strtok(NULL, " ");
		assert(name != NULL);
		char *nl = strchr(name, '\n');
		if(nl)
			*nl = 0;

		//	fprintf(stderr, ":: %c: %s %s :: %lx %d\n", typestr[0], name, idstr, (long)id, sym);
		struct twz_name_ent ent = {
			.id = id,
			.flags = NAME_ENT_VALID,
			.type = type,
			.dlen = strlen(name) + 1 /* null terminator */
		};

		if(sym) {
			ent.dlen += strlen(symstr) + 1;
		}

		size_t rem = ((ent.dlen + 15) & ~15) - ent.dlen;
		char null[16] = {};

		fwrite(&ent, sizeof(ent), 1, f);
		fwrite(name, 1, strlen(name) + 1, f);
		if(sym) {
			fwrite(symstr, 1, strlen(symstr) + 1, f);
		}
		fwrite(null, 1, rem, f);
	}

	return 0;
}
