#pragma once

#include <twz/__twz.h>

#include <stdint.h>
#include <twz/_types.h>

#ifdef __cplusplus
extern "C" {
#endif

__must_check int twz_object_kaction(twzobj *obj, long cmd, ...);

__must_check int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags);

__must_check int twz_object_ctl(twzobj *obj, int cmd, ...);

#ifdef __cplusplus
}
#endif
