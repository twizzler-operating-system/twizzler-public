#pragma once

#include <stddef.h>
#include <stdint.h>
#include <twz/obj.h>

#define MAX_ORDER 28
#define MIN_ORDER 4
struct twzoa_header {
	size_t start, end;
	union {
		struct {
			size_t max_order;
			void *flist[MAX_ORDER + 2];
		} bdy;
	};
};

int oa_init(struct object *obj, size_t start, size_t end);
int oa_hdr_init(struct object *obj, struct twzoa_header *h, size_t start, size_t end);
void oa_hdr_free(struct object *obj, struct twzoa_header *hdr, void *p);
void *oa_hdr_alloc(struct object *obj, struct twzoa_header *hdr, size_t s);
void oa_free(struct object *obj, void *p);
void *oa_alloc(struct object *obj, size_t s);
