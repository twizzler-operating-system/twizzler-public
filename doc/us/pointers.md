Pointer Manipulation (us/pointers.md)
====================

Pointers have two forms, referred to as _persistent_ and _dereferencable_. A persistent pointer
(p-ptr) is a
pointer of the form `(fot-entry, offset)`. A dereferncable pointer (d-ptr) is a pointer in a form that the
CPU can directly use to access data behind it. In terms of C, a d-ptr can have a * applied to it,
whereas a p-ptr must first be converted into a d-ptr.

Pointer manipulation routines take d-ptr's by default for pointer arguments.

## twz_ptr_lea

``` {.c}
#include <twz/obj.h>
T *twz_ptr_lea(struct object *obj, T *ptr [p-ptr]);
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

  * `FAULT_FOT`: An error occurred attempting to resolve the specified FOT entry in the pointer.
	Possible info codes:
	  * `FOT_EXCEEDED`: The entry exceeded the number of FOT entries in this object.
	  * `FOT_INVALID`: The FOT entry specified is invalid.
	  * `FOT_NAMEFAIL`: The FOT entry had a name which failed to resolve.
  * `FAULT_ERR`: An error occurred. Possible info codes:
      * `ENOSPC`: Resources for d-ptrs exhausted.



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

## twz_ptr_store

``` {.c}
#include <twz/obj.h>
T *twz_ptr_store(struct object *obj, T *ptr, uint32_t flags);
```

Convert an existing d-ptr to a p-ptr from the perspective of object `obj`. This will (potentially)
insert a new FOT entry with flags `flags`.

### Return Value
Returns a p-ptr on success. This function always succeeds or throws an exception.

## twz_ptr_rebase

``` {.c}
#include <twz/obj.h>
T *twz_ptr_rebase(size_t fe, T *ptr [p-ptr]);
```

Computes a new pointer with FOT entry `fe` and offset specified by `ptr`.

### Return Value
Returns the new pointer.

### Errors
None.

## twz_ptr_make

``` {.c}
#include <twz/obj.h>
T *twz_ptr_make(struct object *obj, objid_t id, T *ptr, uint32_t flags);
```

Similar to `twz_ptr_store`, except take a local pointer `ptr` and an object ID `id` for creating the
p-ptr.

### Return Value
Returns a p-ptr on success. This function always succeeds or throws an exception.


