#pragma once

#include <stddef.h>
#include <stdint.h>
#include <twz/_objid.h>

enum name_ent_type {
	NAME_ENT_REGULAR,
	NAME_ENT_NAMESPACE,
};

#define NAME_ENT_VALID 1

#define NAMESPACE_MAGIC 0xa13a1300bbbbcccc

struct twz_name_ent {
	objid_t id;
	uint32_t flags;
	uint16_t resv0;
	uint8_t type;
	uint8_t resv1;
	size_t dlen;
	char name[];
} __attribute__((packed));

struct twz_namespace_hdr {
	uint64_t magic;
	uint32_t version;
	uint32_t flags;
	struct twz_name_ent ents[];
};

struct object;
int twz_hier_resolve_name(struct object *ns, const char *path, int flags, struct twz_name_ent *ent);
