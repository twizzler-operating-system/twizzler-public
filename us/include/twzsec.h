#pragma once

#include <twz.h>

#define SC_READ  1
#define SC_WRITE 2
#define SC_EXEC  4
#define SC_USE   8

#define CAP_PERM_MASK (SC_READ|SC_WRITE|SC_EXEC|SC_USE)
#define CAP_DEL   0x1000

#define SIGSZ 32

#define DLGDATA_TYPE_DLG 1
#define DLGDATA_TYPE_CAP 2

struct cap {
	unsigned char sig[SIGSZ];
	objid_t target;
	objid_t accessor;
	uint64_t flags;
	uint64_t resv[3];
} __attribute__((packed));

struct dlgdata {
	uint64_t mask;
	uint32_t type;
	uint32_t length;
	char data[];
} __attribute__((packed));

struct dlg {
	unsigned char sig[SIGSZ];
	objid_t delegatee;
	objid_t delegator;
	uint64_t flags;
	uint64_t resv;
	uint64_t mask;
	uint32_t count;
	uint32_t length;
	char data[];
} __attribute__((packed));

int twzsec_attach(objid_t sc, objid_t tid, int flags);
int twzsec_detach(objid_t sc, objid_t tid, void *jmp, int flags);
void twzsec_cap_create(struct cap *c, objid_t accessor, objid_t target, uint64_t flags);
struct object;
int twzsec_ctx_add(struct object *o, void *data, uint32_t type, uint64_t mask, uint32_t length);

