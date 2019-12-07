#pragma once

#include <stddef.h>
#include <stdint.h>
#include <twz/mutex.h>
#include <twz/obj.h>

#define MAX_ORDER 28
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

#define NR_SZS sizeof(size_classes) / sizeof(size_classes[0])

struct twzoa_header {
	size_t start, end;
	union {
		struct {
			size_t max_order;
			void *flist[MAX_ORDER + 2];
		} bdy;
		struct {
			size_t pgbm_len, last;
			struct {
				struct slab *partial;
			} lists[NR_SZS];
			struct page *pg_partial;
		} slb;
	};
	struct mutex m;
};

int oa_init(twzobj *obj, size_t start, size_t end);
int oa_hdr_init(twzobj *obj, struct twzoa_header *h, size_t start, size_t end);
void oa_hdr_free(twzobj *obj, struct twzoa_header *hdr, void *p);
void *oa_hdr_alloc(twzobj *obj, struct twzoa_header *hdr, size_t s);
void oa_free(twzobj *obj, void *p);
void *oa_alloc(twzobj *obj, size_t s);
