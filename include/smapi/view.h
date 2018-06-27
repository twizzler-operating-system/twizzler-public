#pragma once

#define VE_READ 1
#define VE_WRITE 2
#define VE_EXEC 4
#define VE_VALID 0x80000000

/* TODO (major): determine these from arch */
#define MAX_SLOT 0x1fff0
#define OBJ_SLOTSIZE (1024ul * 1024 * 1024)

struct viewentry {
	objid_t id;
	uint64_t res0;
	_Atomic uint32_t flags;
	uint32_t res1;
} __packed;

