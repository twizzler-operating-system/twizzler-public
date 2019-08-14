Naming in Twizzler (us/names.md)
==========================

_"But Taborlin knew the names of all things, and so all things were his to command."_ - The Name of the Wind

Naming in Twizzler is fundamentally a flexible concept. The idea of a name is not actually known by
the _kernel_, nor most of the userspace APIs. They usually operate on pointers and IDs. Names must
be first resolved into an ID to be _really_ useful. The primary location names show up are in FOT
entries, where the system resolves the name (according to the supplied name resolver) into an ID.

A name resolver is a function which takes a null-terminated string of bytes and returns an ID (or an
error). A name is an unspecified C string.

## twz_name_resolve
``` {.c}
#include <twz/name.h>
int twz_name_resolve(struct object *obj,
  const char *name,
  int (*fn)(struct object *, const char *, int, objid_t *),
  int flags,
  objid_t *id);
```

Resolve a name into an ID. If `obj` is NULL, then `name` is a d-ptr to a C string. If `obj` is _not_
NULL, then `name` is a p-ptr with respect to `obj`. The `fn` argument is a name resolver function to
call (or NULL), whether a d-ptr or a p-ptr following the same rules as `name`.
The flags argument is passed through to the resolving function.

If `fn` is NULL, then use the Twizzler Default Name Resolver. This uses the `TWZNAME` envvar to
determine which object contains the system's default name object, and looks up the name in that.

The Default resolver ignores the `flags` argument.

### Return Value
The resulting object ID is returned in `id` and the function returns 0 on success. On error, an
error code is returned. If the called name resolver returns an error, that error is passed through.

### Errors

Error codes include any error from the `fn`, but also:
* `-EINVAL`: Invalid argument
* `-ENOENT`: Name not found
* `-EFAULT`: Could not load pointers either `name` or `fn` from `obj`.

## twz_name_assign
``` {.c}
#include <twz/name.h>
int twz_name_assign(objid_t id, const char *name);
```

Assign `name` to `id` in the Twizzler Default Name Resolver. Multiple names for an ID are allowed.

### Return Value
Returns 0 on success, error code on error.

### Errors
* `-ENOENT`: Could not find a name object.
* `-ENOMEM`: Not enough memory to assign name.

## twz_name_reverse_lookup

``` {.c}
#include <twz/name.h>
int twz_name_reverse_lookup(objid_t id,
  char *name,
  size_t *nl,
  ssize_t (*fn)(objid_t id, char *name, size_t *nl, int flags),
  int flags);
```

Reverse-lookup a name that refers to `id`,
using the provided function `fn`. If `fn` is NULL, use the Twizzler Default
Name Resolver. The `flags` argument is passed through to the `fn`. The `nl` argument points to a
`size_t` that contains the number of bytes available to write to in `name`. The function will not
write more than this many bytes in `name`, including the NULL terminator. After returning, `nl` is
updated to indicate the length of the name. If multiple names refer to `id`, there is no requirement
that any particular one is returned. If `nl` does not specify enough bytes to hold the name, `nl` is
updated to indicate how much memory is needed, and `-ENOMEM` is returned.

The `fn` argument takes the object ID, a buffer for the name, the length (via `nl`), and flags.
It returns the actual length of the name on success, and error code on error.

The Default resolver ignores the `flags` argument.

### Return Value

Returns 0 on success, error code on error.

### Errors
Can return any error that `fn` returns, along with:

* `-EINVAL`: Invalid argument.
* `-ENOMEM`: Not enough space to hold the name.
* `-ENOENT`: No name found.



