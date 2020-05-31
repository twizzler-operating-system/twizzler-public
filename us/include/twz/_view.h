#pragma once

#include <twz/__cpp_compat.h>

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least32_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

#include <assert.h>
#include <stddef.h>
#include <twz/_kso.h>
#include <twz/_obj.h>

#define VE_READ FE_READ
#define VE_WRITE FE_WRITE
#define VE_EXEC FE_EXEC
#define VE_VALID 0x1000
#define VE_FIXED 0x2000

#define __VE_OFFSET (KSO_NAME_MAXLEN + 16)

struct viewentry {
	objid_t id;
	uint64_t res0;
	atomic_uint_least32_t flags;
	uint32_t res1;
};

static_assert(sizeof(struct viewentry) == 32, "");
static_assert(offsetof(struct viewentry, flags) == 24, "");
