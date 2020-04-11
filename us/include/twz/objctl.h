#pragma once

#include <twz/obj.h>

int twz_object_kaction(twzobj *obj, long cmd, ...);
int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags);
int twz_object_ctl(twzobj *obj, int cmd, ...);
