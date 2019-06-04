#pragma once

#include <twz/_objid.h>
enum {
	FAULT_OBJECT,
	FAULT_NULL,
	FAULT_EXCEPTION,
	NUM_FAULTS,
};

struct faultinfo {
	objid_t view;
	void *addr;
	uint64_t flags;
} __packed;

#define FAULT_OBJECT_READ 1
#define FAULT_OBJECT_WRITE 2
#define FAULT_OBJECT_EXEC 4
#define FAULT_OBJECT_NOMAP 8
#define FAULT_OBJECT_EXIST 16

struct fault_object_info {
	objid_t objid;
	uint64_t ip;
	uint64_t addr;
	uint64_t flags;
	uint64_t pad;
} __packed;

struct fault_null_info {
	uint64_t ip;
	uint64_t addr;
} __packed;

struct fault_exception_info {
	uint64_t ip;
	uint64_t code;
	uint64_t arg0;
} __packed;
