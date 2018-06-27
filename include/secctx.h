#pragma once

#include <arch/secctx.h>

struct secctx {
	struct arch_secctx arch;
	objid_t repr;
};

