#pragma once

#include <arch/secctx.h>
#include <krc.h>

#include <twz/_objid.h>

struct sctx {
	struct arch_sctx arch;
	objid_t repr;
	struct krc refs;
};

void arch_secctx_init(struct sctx *sc);
void arch_secctx_destroy(struct sctx *sc);

struct sctx *secctx_alloc(objid_t repr);
void secctx_free(struct sctx *s);
