#pragma once

#include <stddef.h>
#include <twz/_kso.h>
#include <twz/_obj.h>
#include <twz/_objid.h>

struct screvoc {
	uint64_t create;
	uint64_t valid;
};

struct scgates {
	uint32_t offset;
	uint16_t length;
	uint16_t align; /* 2^align, so align=3 means 8-byte alignment */
};

enum SC_HASH_FNS {
	SCHASH_SHA1,
	SCHASH_NUM,
};

#define SCHASH_DEFAULT SCHASH_SHA1

enum SC_ENC_FNS {
	SCENC_DSA,
	SCENC_NUM,
};

#define SCENC_DEFAULT SCENC_DSA

#define SCP_READ MIP_DFL_READ
#define SCP_WRITE MIP_DFL_WRITE
#define SCP_EXEC MIP_DFL_EXEC
#define SCP_USE MIP_DFL_USE
#define SCP_DEL MIP_DFL_DEL
#define SCP_CD 0x1000

#define SCF_TYPE_REGRANT_MASK 2
#define SCF_GATE 2
#define SCF_REV 4

#define SC_CAP_MAGIC 0xca55
#define SC_DLG_MAGIC 0xdf55

/* Effective permissions (EP) of an object are calculated as follows:
 * P0 = ((obj.pflags & ~sc.gmask) | (obj.pflags & sc[obj].amask)) & ~sc[obj].mask
 * EP = P0 | all-caps | all-dlgs
 *
 * Basically, the object's default permissions are masked by a global mask
 * (a way to make the context a "granted only" context). Then an objects default permissions
 * are masked by a per-object mask in the context (to "add back in" permissions per object).
 * Finally, the permissions are masked by a per-object mask. The reasons for this:
 *   - I may want a security context that gives access to object only via caps (hence, global mask).
 *   - I may want a context that gives executable access via caps only, except I can also execute a
 *     few other object that normally would be executable (since the per-object regranting mask).
 *   - Finally, I may want to allow normal access to object, but prevent reading from a specific one
 *     (since the final object mask).
 */

struct sccap {
	objid_t target;
	objid_t accessor;
	struct screvoc rev;
	struct scgates gates;
	uint32_t perms;
	uint16_t magic;
	uint16_t flags;
	uint16_t htype;
	uint16_t etype;
	uint16_t slen;
	uint16_t pad;
	char sig[];
} __attribute__((packed));

struct scdlg {
	objid_t delegatee;
	objid_t delegator;
	struct screvoc rev;
	struct scgates gates;
	uint32_t mask;
	uint16_t magic;
	uint16_t flags;
	uint16_t htype;
	uint16_t etype;
	uint16_t slen;
	uint16_t dlen;
	char data[];
} __attribute__((packed));

_Static_assert(sizeof(struct scdlg) == sizeof(struct sccap),
  "CAP and DLG struct size must be the same");

_Static_assert(offsetof(struct scdlg, magic) == offsetof(struct sccap, magic),
  "CAP and DLG struct magic offset must be the same");

struct scbucket {
	objid_t target;
	void *data;
	uint64_t flags;
	struct scgates gatemask;
	uint32_t pmask;
	uint32_t chain;
};

struct secctx {
	struct kso_hdr hdr;
	union {
		char userdata[512];
		struct {
			size_t max;
		} alloc;
	};
	uint32_t gmask;
	uint32_t flags;
	size_t nbuckets;
	size_t nchain;
	struct scbucket buckets[];
};
