#pragma once
#include <stdint.h>
#include <twz/_objid.h>
struct object;
int twz_view_get(struct object *obj, size_t slot, objid_t *target, uint32_t *flags);
int twz_view_set(struct object *obj, size_t slot, objid_t target, uint32_t flags);
