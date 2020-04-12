#pragma once

#include <twz/alloc.h>

#define MAX_ORDER NR_SZS
#define MIN_ORDER 4

struct slab {
	struct slab *next;
	uint64_t alloc[4];
	uint16_t sz;
	uint16_t nrobj;
	uint32_t pad; // needed for first element's meta
};

#define PAGE_SIZE 4096

struct page {
	struct slab *slab[7];
};

static const uint16_t size_classes[] = {
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	9,
	10,
	12,
	15,
	18,
	21,
	25,
	31,
	36,
	42,
	51,
	63,
	73,
	85,
	102,
	127,
	146,
	170,
	204,
	255,
};

_Static_assert(NR_SZS == sizeof(size_classes) / sizeof(size_classes[0]), "");
