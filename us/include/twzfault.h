#pragma once

#include <stdint.h>
#include <twz.h>

#define FAULT_OBJECT_READ  1
#define FAULT_OBJECT_WRITE 2
#define FAULT_OBJECT_EXEC  4
#define FAULT_OBJECT_NOMAP 8
#define FAULT_OBJECT_EXIST 16

struct fault_object_info {
	objid_t objid;
	uint64_t ip;
	uint64_t addr;
	uint64_t flags;
	uint64_t pad;
};

struct fault_null_info {
	uint64_t ip;
	uint64_t addr;
};

struct fault_exception_info {
	uint64_t ip;
	uint64_t code;
	uint64_t arg0;
};

void twz_fault_set(int fault, void (*fn)(int, void *));

