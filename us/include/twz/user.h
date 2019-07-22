#pragma once

#include <stdint.h>
#include <twz/_objid.h>

struct user_hdr {
	objid_t dfl_keyring;
	objid_t dfl_secctx;
	char *name;
	uint64_t flags;
};
