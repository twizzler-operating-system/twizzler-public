#pragma once

#include <twz/_obj.h>
#include <twz/_objid.h>

struct screvoc {
	uint64_t create;
	uint64_t valid;
};

struct scgates {
	uint32_t offset;
	uint16_t length;
	uint16_t align;
};

enum SC_HASH_FNS {
	SCHASH_BLAKE2,
};

enum SC_ENC_FNS {
	SCENC_RSA,
};

#define SCP_READ MIP_DFL_READ
#define SCP_WRITE MIP_DFL_WRITE
#define SCP_EXEC MIP_DFL_EXEC
#define SCP_USE MIP_DFL_USE
#define SCP_CD 0x1000

#define SCF_TYPE_DLG 1
#define SCF_GATE 2
#define SCF_REV 4

struct sccap {
	objid_t target;
	objid_t accessor;
	struct screvoc rev;
	struct scgates gates;
	uint32_t perms;
	uint32_t pad;
	uint16_t flags;
	uint16_t htype;
	uint16_t etype;
	uint16_t slen;
	char sig[];
};

struct scdlg {
	objid_t delegatee;
	objid_t delegator;
	struct screvoc rev;
	struct scgates gates;
	uint32_t mask;
	uint16_t flags;
	uint16_t htype;
	uint16_t etype;
	uint16_t pad;
	uint16_t slen;
	uint16_t dlen;
	char data[];
};

struct secctx {
};
