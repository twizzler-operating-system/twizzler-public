#pragma once

#include <twz/_kso.h>
#include <twz/_obj.h>

#define VE_READ FE_READ
#define VE_WRITE FE_WRITE
#define VE_EXEC FE_EXEC
#define VE_VALID 0x1000
#define VE_FIXED 0x2000

#define __VE_OFFSET (KSO_NAME_MAXLEN + 8)

struct viewentry {
	objid_t id;
	uint64_t res0;
	_Atomic uint32_t flags;
	uint32_t res1;
} __attribute__((packed));
