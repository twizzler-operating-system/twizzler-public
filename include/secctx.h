#pragma once

#include <arch/secctx.h>
#include <krc.h>

#include <twz/_objid.h>

struct secctx {
	struct arch_secctx arch;
	objid_t repr;
	struct krc refs;
};

void arch_secctx_init(struct secctx *sc);
void arch_secctx_destroy(struct secctx *sc);

struct secctx *secctx_alloc(objid_t repr);
void secctx_free(struct secctx *s);
