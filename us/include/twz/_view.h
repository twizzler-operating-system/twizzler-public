#pragma once

#include <twz/_obj.h>

#define VE_READ FE_READ
#define VE_WRITE FE_WRITE
#define VE_EXEC FE_EXEC

struct viewentry {
	objid_t id;
	uint64_t res0;
	_Atomic uint32_t flags;
	uint32_t res1;
} __packed;
