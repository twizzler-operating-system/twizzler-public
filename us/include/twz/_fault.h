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

/* object faults: faults that happen due to accessing an object in the virtual space. These include:
 *   - nothing mapped
 *   - protection errors
 *   - invalid view entry
 *   - object doesn't exist
 */

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

static inline struct fault_object_info twz_fault_build_object_info(objid_t id,
  uint64_t ip,
  uint64_t addr,
  uint64_t flags)
{
	struct fault_object_info fi;
	fi.objid = id;
	fi.ip = ip;
	fi.addr = addr;
	fi.flags = flags;
	fi.pad = 0;
	return fi;
}

/* null faults: happen when access occurs in a valid object slot in the null page. */

struct fault_null_info {
	uint64_t ip;
	uint64_t addr;
} __attribute__((packed));

static inline struct fault_null_info twz_fault_build_null_info(uint64_t ip, uint64_t addr)
{
	struct fault_null_info fi;
	fi.ip = ip;
	fi.addr = addr;
	return fi;
}

/* exception faults: occur due to CPU exceptions (GPF, DIV0, etc). */

struct fault_exception_info {
	uint64_t ip;
	uint64_t code; /* architecture specific */
	uint64_t arg0; /* architecture specific */
} __attribute__((packed));

static inline struct fault_exception_info twz_fault_build_exception_info(uint64_t ip,
  uint64_t code,
  uint64_t arg0)
{
	struct fault_exception_info fi;
	fi.ip = ip;
	fi.code = code;
	fi.arg0 = arg0;
	return fi;
}

/* security faults: occur due to a permissions error */

struct fault_sctx_info {
	objid_t target;
	uint64_t ip;
	uint64_t addr;
	uint32_t pneed;
	uint32_t pad;
	uint64_t pad64;
} __attribute__((packed));

static inline struct fault_sctx_info twz_fault_build_sctx_info(objid_t target,
  uint64_t ip,
  uint64_t addr,
  uint32_t pneed)
{
	struct fault_sctx_info fi;
	fi.target = target;
	fi.ip = ip;
	fi.addr = addr;
	fi.pneed = pneed;
	fi.pad = 0;
	fi.pad64 = 0;
	return fi;
}

/* double faults: occur when a fault was generated while handling another fault. Generally fatal. */

struct fault_fault_info {
	uint32_t fault_nr;
	uint32_t info;
	uint32_t len;
	uint32_t resv;
	char data[];
} __attribute__((packed));

static inline struct fault_fault_info twz_fault_build_fault_info(uint32_t fault_nr,
  uint32_t info,
  uint32_t len)
{
	struct fault_fault_info fi;
	fi.fault_nr = fault_nr;
	fi.info = info;
	fi.len = len;
	fi.resv = 0;
	return fi;
}

/* page faults: generated when a page of an object is requested, but cannot be found in-kernel, and
 * a handler is setup */

struct fault_page_info {
	objid_t objid;
	uintptr_t vaddr;
	size_t pgnr;
	uint64_t info;
	uintptr_t ip;
} __attribute__((packed));

static inline struct fault_page_info twz_fault_build_page_info(objid_t id,
  uintptr_t vaddr,
  size_t pagenr,
  uint64_t info,
  uintptr_t ip)
{
	struct fault_page_info fi;
	fi.objid = id;
	fi.vaddr = vaddr;
	fi.pgnr = pagenr;
	fi.info = info;
	fi.ip = ip;
	return fi;
}

/* pointer faults: occur during translation of a p-ptr to a v-ptr. They can happen because:
 *    - name resolution failure
 *    - lack of resources
 *    - invalid object or pointer
 *    - failed to derive new object
 */

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

static inline struct fault_pptr_info twz_fault_build_pptr_info(objid_t id,
  size_t fote,
  uintptr_t ip,
  uint32_t info,
  uint32_t retval,
  uint64_t flags,
  const char *name,
  const void *ptr)
{
	struct fault_pptr_info fi;
	fi.objid = id;
	fi.fote = fote;
	fi.ip = ip;
	fi.info = info;
	fi.retval = retval;
	fi.flags = flags;
	fi.name = name;
	fi.ptr = ptr;
	return fi;
}
