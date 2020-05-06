Object API (us/objects.md)
==========


Twizzler objects have a maximum size (found in `OBJ_MAXSIZE`). Objects are broken into two regions:
_data_ and _metadata_. This distinction is largely irrelevant, except that `libtwz` often
manipulates metadata, and metadata grows downward from the top of the object to the end of the data
region (`OBJ_TOPDATA`). The data region starts at 0 and grows upward.
Near the top of the object (in a known location) is a structure of type `struct metainfo`. This
struct contains metadata information. Twizzler provides functions to manipulate its contents.

Note that the first page of an object is unmapped (for catching NULL pointers). Twizzler exposes a
constant, `OBJ_NULLPAGE_SIZE`, that indicates how large this is. Normal programs will not need to
use this constant most of the time. All offsets within an object must be larger than this constant.

Objects are referred to by programs through object handles of type twzobj. Typically, Twizzler API
functions will take a pointer to a twzobj handle as an input. You can think of these as similar to
FILE in C, though with much less state.

Most Twizzler functions return an error code directly, instead of the standard C approach of using
errno. The error codes are typically negative numbers from the errno header.
Note that C library functions will still use errno. Some functions are designed to "succeed
or throw", in which case they often return data directly or return void and will silently succeed or
else throw an exception (see Exceptions).

## twz_object_init_guid

``` {.c}
#include <twz/obj.h>
int twz_object_init_guid(twzobj *obj, objid_t id, int flags);
```

Initialize an object handle that refers to the object specified by `id`. The `flags` argument is a
bitfield with the following bits:

* `FE_READ`: Read access requested
* `FE_WRITE`: Write access requested
* `FE_EXEC`: Execute access requested
* `FE_DERIVE`: Open a copy of this object, not the object itself.

If the calling thread does not have the permissions requested, the function might still succeed.
First access to the data that is disallowed will cause a fault.

### Return Value
Returns 0 on success, and error code on error.

### Errors
* `-EINVAL`: Invalid argument.
* `-ENOENT`: Could not locate object `id`. Note that if `id` does not exist, this function is not
  required to return an error.
* `-EACCES`: Tried to request both write and execute permissions simultaneously.
* `-ENOMEM`: Not enough memory to fulfill the request.

## twz_object_init_name

``` {.c}
#include <twz/obj.h>
int twz_object_init_name(twzobj *obj, const char *name, int flags);
```

Initialize an object handle that refers to the object specified by name `name` after being passed to the default name resolver.
The flags are the same as `twz_object_init_guid`.

### Return Value
Returns 0 on success, and error code on error.

### Errors
In addition to the errors returned by `twz_object_init_guid`, this function can return:

* `-ENOENT`: The name was not resolved successfully.
* `-ELOOP`: The name resolution was recursive and too deep.

## twz_object_new

``` {.c}
#include <twz/obj.h>
int twz_object_new(twzobj *newobj, twzobj *srcobj, twzobj *kuid, int flags);
```

Create an object, using newobj as the handle to the new object. The other arguments are as follows:

  * `srcobj`: Copy from a _source object_; that is, the new object will be byte-wise identical to
	the source object specified by this argument (with the exception of some metadata fields). If
	NULL, the new object is initialized to contain zeros, except key metadata info.
  * `kuid`: Give the object a public-key that can be used to verify signatures for capabilities for
	this object (see Security). If NULL, the new object will not have a public-key. If special
	value `TWZ_KU_USER`, the function will assign the new object's public key as the contents of the
	env var `TWZUSERKU`, thus providing a default.
  * `flags` is a bitwise combination of the following:
    * `TWZ_OC_HASHDATA`: When deriving the object ID, hash the object's data. This makes the object
	  immutable. It cannot be combined with the `TWZ_OC_DFL_WRITE` flag.
	* `TWZ_OC_DFL_*`: For values of `*` being `READ`, `WRITE`, `EXEC`, `DEL`, and `USE`, this sets the
	  default permissions of the object. For example, anyone who knows the ID of an object with
	  `TWZ_OC_DFL_WRITE` set can write to the object.
	* `TWZ_OC_VOLATILE`: make this object volatile. A _persistent_ object is guaranteed to be
	  reachable across reboots. A volatile object is _not_ guaranteed this. TODO: is to worth
	  strengthening this?
	* `TWZ_OC_TIED_NONE`: Do not tie this object's lifetime to another object. By default, a new
	  object's lifetime is tied to the creating thread. For more details, see Tying Objects.
	* `TWZ_OC_TIED_VIEW`: Tie the object to the current view ("wire" the object). This flag is
	  incompatible with `TWZ_OC_TIED_NONE`.

### Errors
In addition to errors returned by `sys_ocreate` and `twz_object_init_guid`, this function can return:

* `-EINVAL`: The user specified `TWZ_KU_USER` for `kuid` and Twizzler failed to determine the
  correct GUID for the public-key.

## twz_object_release

``` {.c}
#include <twz/obj.h>
int twz_object_release(twzobj *obj);
```

Destroy this object handle. This call is idempotent for a given handle. This call releases
userspace-level resources for the given object. It may include a system-call, but is not guaranteed
to.

### Return Value

Returns 0 on success, error code on error.

### Errors
* `-EINVAL`: Invalid argument.

## twz_object_base

``` {.c}
#include <twz/obj.h>
void *twz_object_base(twzobj *obj);
```

Return a d-ptr (see Pointer Manipulation; essentially, a pointer which can be dereferenced) that
refers to the base of the object (the first data byte).

### Return Value

On success, return a pointer to the object's data start. Throws `FAULT_PPTR` on error.

## twz_object_meta

``` {.c}
#include <twz/obj.h>
struct metainfo *twz_object_meta(twzobj *obj);
```

Similar to `twz_object_base`, but return a pointer to the metainfo structure instead.

## twz_object_delete

``` {.c}
#include <twz/obj.h>
int twz_object_delete(twzobj *obj, int flags);
```

Delete an object. The object will be deleted by the kernel when all internal references are dropped.
Note that this DOES NOT mean pointers to data within this object. An internal kernel reference just
refers to mappings in the address spaces, etc. If you want to delete an object and keep it around
for some time, see Tying Objects (this lets you do the unlink-after-open trick for automatic cleanup
from Unix). By default, after an object has been deleted by this call but not yet cleaned up (due to
internal references), new references to this object can still be created (it can be opened and used
normally).

Flags is a bitwise combination of the following:

  * `TWZ_OD_IMMEDIATE`: New references cannot be created to this object.

### Return Value

Returns 0 on success, error code on error.

### Errors
See `sys_odelete`.

## Object Metadata

The object's metadata contains three primary compenents:

* The metainfo struct, located at the base of the last page of the object.
* The foreign object table, which contains a list of object IDs used by cross-object pointers to
  refer outside of the object.
* A list of structured object tags (see Structured Objects).


