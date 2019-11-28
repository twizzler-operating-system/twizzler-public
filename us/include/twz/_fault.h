#pragma once

#include <stdint.h>
#include <twz/_objid.h>
enum {
	FAULT_OBJECT,
	FAULT_NULL,
	FAULT_EXCEPTION,
	FAULT_SCTX,
	FAULT_FAULT,
	FAULT_PAGE,
	FAULT_PPTR,
	NUM_FAULTS,
};

struct faultinfo {
	objid_t view;
	void *addr;
	uint64_t flags;
} __attribute__((packed));

#define FAULT_OBJECT_READ 1
#define FAULT_OBJECT_WRITE 2
#define FAULT_OBJECT_EXEC 4
#define FAULT_OBJECT_NOMAP 8
#define FAULT_OBJECT_EXIST 16
#define FAULT_OBJECT_INVALID 32
#define FAULT_OBJECT_UNKNOWN 64

struct fault_object_info {
	objid_t objid;
	uint64_t ip;
	uint64_t addr;
	uint64_t flags;
	uint64_t pad;
} __attribute__((packed));

struct fault_null_info {
	uint64_t ip;
	uint64_t addr;
} __attribute__((packed));

struct fault_exception_info {
	uint64_t ip;
	uint64_t code;
	uint64_t arg0;
} __attribute__((packed));

struct fault_sctx_info {
	objid_t target;
	uint64_t ip;
	uint64_t addr;
	uint32_t pneed;
	uint32_t pad;
	uint64_t pad64;
} __attribute__((packed));

struct fault_fault_info {
	uint32_t fault_nr;
	uint32_t info;
	uint32_t len;
	uint32_t resv;
	char data[];
} __attribute__((packed));

struct fault_page_info {
	objid_t objid;
	uintptr_t vaddr;
	size_t pgnr;
	uint64_t info;
	uintptr_t ip;
} __attribute__((packed));

enum {
	FAULT_PPTR_UNKNOWN,
	FAULT_PPTR_INVALID,
	FAULT_PPTR_RESOLVE,
	FAULT_PPTR_RESOURCES,
	FAULT_PPTR_DERIVE,
	NUM_FAULT_PPTR_INFO
};

struct fault_pptr_info {
	objid_t objid;
	size_t fote;
	uintptr_t ip;
	uint32_t info;
	uint32_t retval;
	uint64_t flags;
	const char *name;
	const void *ptr;
} __attribute__((packed));
