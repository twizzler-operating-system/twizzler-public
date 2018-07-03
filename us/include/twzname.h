#pragma once

#include <twz.h>
#include <twzobj.h>

#define NAME_RESOLVER_DEFAULT 0

objid_t twz_name_resolve(struct object *o, const char *name, uint64_t resolver);
int twz_name_assign(objid_t id, const char *name, uint64_t resolver);

