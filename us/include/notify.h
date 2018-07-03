#pragma once

#include <twzobj.h>
#include <twzsys.h>
#include <twzerr.h>
#include <stdatomic.h>
#include <limits.h>

#define NOTIFY_HEADER_ID 2

#define NH_USED        1
#define NH_VALID       2
#define NH_REG         4
#define NTYPE_READ  0x10
#define NTYPE_WRITE 0x20

#define NTYPE(x) ((x) & (NTYPE_READ|NTYPE_WRITE))

struct notify_svar {
	uint64_t *var;
	_Atomic uint32_t flags;
	uint32_t id;
};

struct notify_header {
	struct metaheader _header;
	struct notify_svar *vars;
	void (*prepare)(struct object *obj);
	int count;
};

static inline int notify_getresult(_Atomic uint64_t *var, uint32_t *id, uint32_t *flags)
{
	uint64_t r = atomic_exchange(var, 0);
	if(r == 0) return -TE_INVALID;
	if(id) *id = (uint32_t)(r & 0xFFFFFFFF);
	if(flags) *flags = 0; //TODO
	return 0;
}

#define NOTIFY_IDSET(id,n) ((id) & (1ul << (n)))

static inline int notify_wait_var(_Atomic uint64_t *var)
{
	int r __attribute__((unused)) = fbsd_sys_umtx(var, UMTX_OP_WAIT, 0);
	/* TODO: return value */
	return 0;
}

static inline int notify_wake_var(_Atomic uint64_t *var)
{
	int r __attribute__((unused)) = fbsd_sys_umtx(var, UMTX_OP_WAKE, INT_MAX);
	/* TODO: return value */
	return 0;
}

int notify_init(struct object *obj, struct notify_svar *start,
		int count, void (*prep)(struct object *));
int notify_wait(struct object *obj, int n, uint64_t val);
int notify_register(struct object *obj, _Atomic uint64_t *wait,
		uint32_t id, uint32_t flags);
int notify_insert(struct object *obj, uint64_t *addr);
int notify_wake_all(struct object *obj, int count, uint32_t flags);
void notify_prepare(struct object *obj);
int notify_wake(struct object *obj, int n, int count);


