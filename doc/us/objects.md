Object API (us/objects.md)
==========

Twizzler objects contain two regions: _data_ and _metadata_. The distinction is not significant,
just that the metadata region starts at the top of the object and grows downward. The last page of
the object (`OBJ_MAXSIZE` - `OBJ_METAPAGE_SIZE`) contains the _metadata info structure_, of type
`struct metainfo`. The lowest page of the object (starting at offset 0, extending for
`OBJ_NULLPAGE_SIZE`) is called the _null page_. This page is never mapped so that null pointers will
fault. Thus all offsets in an object must be at least `OBJ_NULLPAGE_SIZE`.

Many functions in Twizzler operate on a `struct object`. This is necessary to provide pointers with
FOTs to resolve.

## twz_object_open

``` {.c}
#include <twz/obj.h>
int twz_object_open(struct object *obj, objid_t id, int flags);
```

Initialize an object handle that refers to the object specified by `id`. The `flags` argument is a
bitfield with the following bits:

* `FE_READ`: Read access requested
* `FE_WRITE`: Write access requested
* `FE_EXEC`: Execute access requested

If the calling thread does not have the permissions requested, the function may still succeed.

### Return Value
Returns 0 on success, and error code on error.

### Errors
* `-EINVAL`: Invalid argument.
* `-ENOENT`: Could not locate object `id`.
* `-EACCES`: Tried to request both write and execute permissions simultaneously.
* `-ENOMEM`: Not enough memory to fulfill the request.

## twz_object_create

``` {.c}
#include <twz/obj.h>
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);
```

Create an object, returning the new ID. This function is a wrapper around `sys_ocreate`; see that
function for details.

## twz_object_close

``` {.c}
#include <twz/obj.h>
int twz_object_close(struct object *obj);
```

Destroy this object handle. This call is idemopotent for a given handle.

### Return Value

Returns 0 on success, error code on error.

### Errors
* `-EINVAL`: Invalid argument.

## twz_object_base

``` {.c}
#include <twz/obj.h>
void *twz_object_base(struct object *obj);
```

Return a d-ptr (see Pointer Manipulation; essentially, a pointer which can be dereferenced) that
refers to the base of the object (the first data byte).

### Return Value
Returns a pointer to the object's data base on success, NULL on error. This function fails if `obj`
is NULL or refers to an uninitialized or closed object handle.

## twz_object_meta

``` {.c}
#include <twz/obj.h>
struct metainfo *twz_object_meta(struct object *obj);
```

Similar to `twz_object_base`, but return a pointer to the metainfo structure instead.


