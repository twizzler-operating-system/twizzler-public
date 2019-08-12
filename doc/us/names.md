Naming and Name Resolution (us/names.md)
==========================

## twz_name_resolve
``` {.c}
#include <twz/name.h>
int twz_name_resolve(struct object *obj,
  const char *name,
  int (*fn)(struct object *, const char *, int, objid_t *),
  int flags,
  objid_t *id);
```

## twz_name_assign
``` {.c}
#include <twz/name.h>
int twz_name_assign(objid_t id, const char *name);
```

## twz_name_reverse_lookup

``` {.c}
#include <twz/name.h>
int twz_name_reverse_lookup(objid_t id,
  char *name,
  size_t *nl,
  ssize_t (*fn)(objid_t id, char *name, size_t *nl, int flags),
  int flags);
```
