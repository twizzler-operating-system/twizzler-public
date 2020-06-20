#pragma once

#include <twz/__twz.h>

#include <twz/_objid.h>
#include <twz/_types.h>

#ifdef __cplusplus
extern "C" {
#endif

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

ssize_t twz_name_dfl_getnames(const char *startname, struct twz_nament *ents, size_t len);
twzobj *twz_name_get_root(void);

struct fotentry;
int twz_fot_indirect_resolve(twzobj *obj,
  struct fotentry *fe,
  const void *p,
  void **vptr,
  uint64_t *info);

#define TWZ_NAME_RESOLVER_DFL NULL
#define TWZ_NAME_RESOLVER_HIER NULL
#define TWZ_NAME_RESOLVER_SOFN (void *)1ul

int twz_name_assign_namespace(objid_t id, const char *name);
int twz_name_switch_root(twzobj *obj);
#ifdef __cplusplus
}
#endif
