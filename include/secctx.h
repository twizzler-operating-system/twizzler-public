#pragma once

#include <arch/secctx.h>
#include <krc.h>
#include <object.h>

#include <twz/_objid.h>

struct sctx {
	struct object_space space;
	struct object *obj;
	struct krc refs;
	bool superuser;
};

void arch_secctx_init(struct sctx *sc);
void arch_secctx_destroy(struct sctx *sc);

struct sctx *secctx_alloc(struct object *);
void secctx_free(struct sctx *s);
void secctx_switch(int i);
struct thread;
int secctx_fault_resolve(void *ip,
  uintptr_t loaddr,
  void *vaddr,
  struct object *target,
  uint32_t flags,
  uint64_t *perms);
struct object;
int secctx_check_permissions(void *, struct object *, uint32_t flags);
