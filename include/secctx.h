#pragma once

#include <arch/secctx.h>
#include <krc.h>

#include <twz/_objid.h>

struct sctx {
	struct arch_sctx arch;
	objid_t repr;
	struct krc refs;
	bool superuser;
};

void arch_secctx_init(struct sctx *sc);
void arch_secctx_destroy(struct sctx *sc);

struct sctx *secctx_alloc(objid_t repr);
void secctx_free(struct sctx *s);
void secctx_switch(int i);
struct thread;
void secctx_become_detach(struct thread *thr);
bool secctx_detach_all(struct thread *thr, int flags);
int secctx_fault_resolve(struct thread *t,
  uintptr_t ip,
  uintptr_t loaddr,
  uintptr_t vaddr,
  objid_t target,
  uint32_t flags,
  uint64_t *perms);
