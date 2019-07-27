#pragma once

#include <stdint.h>
#include <twz/_objid.h>
struct kso_attachment {
	objid_t id;
	uint64_t info;
	uint32_t type;
	uint32_t flags;
};

struct kso_root_repr {
	size_t count;
	uint64_t flags;
	struct kso_attachment attached[];
};

#define KSO_ROOT_ID 1
