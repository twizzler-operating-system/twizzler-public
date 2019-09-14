#pragma once

#include <stdint.h>
#include <twz/_objid.h>

enum name_ent_type {
	NAME_ENT_REGULAR,
	NAME_ENT_NAMESPACE,
};

#define NAME_ENT_VALID 1

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
};
