#include <stdbool.h>
#include <stdlib.h>
#include <twz/_err.h>
#include <twz/obj.h>

static bool __name_init(void)
{
	static const char *nameid = NULL;
	if(!nameid)
		nameid = getenv("TWZNAME");
	return !!nameid;
}

static int __twz_name_dfl_resolve(struct object *obj, const char *name, int flags, objid_t *id)
{
	if(!__name_init())
		return -ENOTSUP;

	return -ENOTSUP;
}

int twz_name_resolve(struct object *obj,
  const char *name,
  int (*fn)(struct object *, const char *, int, objid_t *),
  int flags,
  objid_t *id)
{
	if(fn)
		return fn(obj, name, flags, id);
	return __twz_name_dfl_resolve(obj, name, flags, id);
}
