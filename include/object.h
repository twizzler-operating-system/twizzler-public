#pragma once

#include <lib/inthash.h>

struct object {
	uint128_t id;
	size_t maxsz, dataoff;

	int pglevel;
	ssize_t slot;

	struct ihelem elem;
};

void obj_create(uint128_t id, size_t maxsz, size_t dataoff);

