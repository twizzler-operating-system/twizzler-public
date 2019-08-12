Object API (us/objects.md)
==========

## twz_object_open

``` {.c}
#include <twz/obj.h>
int twz_object_open(struct object *obj, objid_t id, int flags);
```

## twz_object_create

``` {.c}
#include <twz/obj.h>
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);
```

## twz_object_close

``` {.c}
#include <twz/obj.h>
int twz_object_open(struct object *obj, objid_t id, int flags);
```

## twz_object_base

``` {.c}
#include <twz/obj.h>
void *twz_object_base(struct object *obj);
```

## twz_object_meta

``` {.c}
#include <twz/obj.h>
struct metainfo *twz_object_meta(struct object *obj);
```




