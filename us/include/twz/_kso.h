#pragma once

#include <stdint.h>
#include <twz/_objid.h>

#define KSO_NAME_MAXLEN 1024

struct kso_attachment {
	objid_t id;
	uint64_t info;
	uint32_t type;
	uint32_t flags;
};

struct kso_hdr {
	char name[KSO_NAME_MAXLEN];
	uint32_t version;
	uint32_t resv;
	uint64_t resv2;
};

struct kso_root_repr {
	struct kso_hdr hdr;
	size_t count;
	uint64_t flags;
	struct kso_attachment attached[];
};

#define KSO_ROOT_ID 1
