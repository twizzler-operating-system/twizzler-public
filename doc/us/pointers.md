Pointer Manipulation (us/pointers.md)
====================

Pointers have two forms, referred to as _persistent_ and _dereferencable_. A persistent pointer
(p-ptr) is a
pointer of the form `(fot-entry, offset)`. A dereferencable pointer (d-ptr) is a pointer in a form that the
CPU can directly use to access data behind it. In terms of C, a d-ptr can have a * applied to it,
whereas a p-ptr must first be converted into a d-ptr.

Note that some internal code may still refer to d-ptr as v-ptr.

Pointer manipulation routines take d-ptr's by default for pointer arguments.

## twz_object_lea

``` {.c}
#include <twz/obj.h>
T *twz_object_lea(twzobj *obj, T *ptr [p-ptr]);
```
Computes a d-ptr from a p-ptr. The `obj` argument must not be NULL, and refers to the object whose
FOT will be used for the computation. The method of computation is platform-specific, and could be a
no-op. If `ptr` is NULL, no computation is performed, and NULL is returned.

### Return Value

On successful computation, a d-ptr that accesses the data specified by the p-ptr is returned. The
d-ptr is valid for the same lifetime as `obj`.

### Errors

This function does not return errors, and will either succeed or raise a fault. The possible faults
can be:

  * `FAULT_PPTR`: An error occurred attempting to resolve the specified FOT entry in the pointer.
	Possible info codes:
	  * `FAULT_PPTR_RESOURCES`: The entry exceeded the number of FOT entries in this object.
	  * `FAULT_PPTR_RESOURCES`: Resources exhausted for d-ptrs.
	  * `FAULT_PPTR_INVALID`: The FOT entry specified is invalid.
	  * `FAULT_PPTR_RESOLVE`: The FOT entry had a name which failed to resolve.

## twz_ptr_local

``` {.c}
#include <twz/obj.h>
T *twz_ptr_local(T *ptr [p-ptr]);
```

Computes the offset into the object that the ptr refers to, essentially converts the pointer into a
local pointer.

### Return Value

Returns a local p-ptr (fot-entry = 0) for the specified pointer.

### Errors

None.


## twz_ptr_store_guid

``` {.c}
#include <twz/obj.h>
int twz_ptr_store_guid(twzobj *obj, const T **loc, twzobj *target,
                      const T *p, uint64_t flags);
```

Create a p-ptr from `p`, store it in the location pointed to by `loc` (which must be a d-ptr, and
must refer to a location within the object specified by `obj`). If `target` is NULL, `p` is
interpreted as a d-ptr, otherwise `p` is interpreted as a p-ptr within object `target`. The `flags`
argument specifies the flags to store in the FOT entry for this pointer (see twz_object_init_guid).

### Return Value

Returns 0 on success, error code on error.

### Errors

  * `-ENOENT`: `p` was a d-ptr, but could not be matched to an existing object.
  * `-EINVAL`: Invalid argument.
  * `-ENOSPC`: No more FOT entries are available in `obj`.

## twz_ptr_store_name

``` {.c}
#include <twz/obj.h>
int twz_ptr_store_name(twzobj *obj, const T **loc, const char *name,
                      const T *p, uint64_t flags);
```

WARNING - this API is unstable.

Create a p-ptr from `p`, store it in the location pointed to by `loc` (which must be a d-ptr, and
must refer to a location within the object specified by `obj`). The FOT entry for this p-ptr will
refer to an object by name `name`. `p` must be a p-ptr. See `twz_ptr_store_guid` for flags.

### Return Value

Returns 0 on success, error code on error.

### Errors

  * `-EINVAL`: Invalid argument.
  * `-ENOSPC`: No more FOT entries are available in `obj`.

## twz_ptr_swizzle

``` {.c}
#include <twz/obj.h>
[p-ptr] T *twz_ptr_swizzle(twzobj *obj, T *p, uint64_t flags);
```

Create and return a p-ptr that can be stored into object `obj`. This manipulates the FOT. `p` must
be a d-ptr. See `twz_ptr_store_guid` for flags.

### Return Value

Returns a p-ptr from that refers to the data pointed to by `p`. Throws on error.

### Errors

  * `FAULT_PPTR`: An error occurred attempting to resolve the specified FOT entry in the pointer.
	Possible info codes:
	  * `FAULT_PPTR_RESOURCES`: Out of FOT entries.
	  * `FAULT_PPTR_INVALID`: `p` was not a valid d-ptr.


