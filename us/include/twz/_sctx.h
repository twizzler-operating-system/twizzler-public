#pragma once

#include <stddef.h>
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

enum SC_ENC_FNS {
	SCENC_DSA,
	SCENC_NUM,
};

#define SCP_READ MIP_DFL_READ
#define SCP_WRITE MIP_DFL_WRITE
#define SCP_EXEC MIP_DFL_EXEC
#define SCP_USE MIP_DFL_USE
#define SCP_DEL MIP_DFL_DEL
#define SCP_CD 0x1000

#define SCF_TYPE_DLG 1
#define SCF_GATE 2
#define SCF_REV 4

#define SC_CAP_MAGIC 0xca55
#define SC_DLG_MAGIC 0xdf55

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
};

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
};

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
	union {
		char userdata[512];
		struct {
			size_t max;
		} alloc;
	};
	size_t nbuckets;
	size_t nchain;
	struct scbucket buckets[];
};
