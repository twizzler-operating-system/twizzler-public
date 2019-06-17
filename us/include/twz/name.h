#pragma once
#include <twz/_objid.h>
struct object;
int twz_name_resolve(struct object *obj,
  const char *name,
  int (*fn)(struct object *, const char *, int, objid_t *),
  int flags,
  objid_t *id);
int twz_name_assign(objid_t id, const char *name);
