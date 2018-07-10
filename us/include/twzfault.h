#pragma once

#include <stdint.h>

#define FAULT_OBJECT_READ  1
#define FAULT_OBJECT_WRITE 2
#define FAULT_OBJECT_EXEC  4
#define FAULT_OBJECT_NOMAP 8
#define FAULT_OBJECT_NULL  0x10

struct fault_object_info {
	uint64_t ip;
	uint64_t addr;
	uint64_t flags;
	uint64_t pad;
};


