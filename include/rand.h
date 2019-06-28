#pragma once
#include <lib/list.h>
struct entropy_source {
	const char *name;
	size_t uses;
	ssize_t (*get)(void *, size_t);
	struct list entry;
};

int rand_getbytes(void *, size_t, int);
void rand_register_entropy_source(struct entropy_source *src);

void rand_csprng_get(void *data, size_t len);
void rand_csprng_reseed(void *entropy, size_t len);
#define RANDSIZL (8)
#define RANDSIZ (1 << RANDSIZL)
