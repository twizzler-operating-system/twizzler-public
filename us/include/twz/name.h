#pragma once

#include <twz/__twz.h>

#include <twz/_objid.h>
#include <twz/_types.h>

struct twz_nament {
	objid_t id;
	size_t reclen;
	uint64_t flags;
	char name[];
};

__must_check int twz_name_resolve(twzobj *obj,
  const char *name,
  int (*fn)(twzobj *, const char *, int, objid_t *),
  int flags,
  objid_t *id);
int twz_name_assign(objid_t id, const char *name);

/* TODO: remove this */
int twz_name_reverse_lookup(objid_t id,
  char *name,
  size_t *nl,
  ssize_t (*fn)(objid_t id, char *name, size_t *nl, int flags),
  int flags);
ssize_t twz_name_dfl_getnames(const char *startname, struct twz_nament *ents, size_t len);
twzobj *twz_name_get_root(void);
