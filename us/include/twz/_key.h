#pragma once

#include <twz/_obj.h>

#define TWZ_KEY_PRI 1

struct key_hdr {
	unsigned char *keydata;
	size_t keydatalen;
	uint32_t type;
	uint32_t flags;
};
