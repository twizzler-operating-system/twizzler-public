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

## twz_object_fot

``` {.c}
#include <twz/obj.h>
struct fotentry *twz_object_fot(struct object *obj);
```

Similar to `twz_object_meta`, bur return a pointer to the base of the FOT instead. The length of the
FOT can be found in the metainfo struct, see Object Metadata.

## Object Metadata

The object's metadata contains three primary compenents:

* The metainfo struct, located at the base of the last page of the object.
* The foreign object table, which contains a list of object IDs used by cross-object pointers to
  refer outside of the object.
* A list of structured object tags (see Structured Objects).

The metainfo struct is defined as follows:
``` {.c}
struct metainfo {
	uint32_t magic;      /* magic number, set to MI_MAGIC */
	uint16_t flags;      /* non-secure flags */
	uint16_t p_flags;    /* secure flags */
	uint16_t milen;      /* length of the metainfo structure */
	uint16_t fotentries; /* number of FOT entries */
	uint32_t mdbottom;   /* bottom of the metadata region, stored as
	                      * offset from the end of the object */
	uint64_t sz;         /* size of the data region, optional */
	uint32_t reserved0;
	uint32_t reserved1;
	nonce_t nonce;       /* a nonce, used for generating IDs */
	objid_t kuid;        /* object ID of the public key for this
	                      * object (see Security). */
} __attribute__((packed));
```

Valid bits for `flags` include:

* `MIF_SZ`: The `sz` field is valid.

valid bits for `p_flags` include:

* `MIP_DFL_READ`: Object's default access rights include 'read'.
* `MIP_DFL_WRITE`: Object's default access rights include 'write'.
* `MIP_DFL_EXEC`: Object's default access rights include 'exec'.
* `MIP_DFL_USE`: Object's default access rights include 'use'.
* `MIP_HASHDATA`: Object's ID is derived in-part by the contents of the object.

The FOT is an array of FOT entries, each structured as follows:

``` {.c}
struct fotentry {
	union {
		objid_t id;
		struct {
			char *data; /* p-ptr to a name */
			void *nresolver; /* p-ptr to a name resolver */
		} name;
	}; /* target object ID or name */

	uint64_t flags;
	uint64_t info; /* for future use */
} __attribute__((packed));
```

An `id` of 0 and `flags` of 0 indicates the entry is invalid.

Valid bits for `flags` include:

* `FE_READ`: Request read access to the target object.
* `FE_WRITE`: Request write access to the target object.
* `FE_EXEC`: Request exec access to the target object.
* `FE_NAME`: The entry is a name rather than an ID.
* `FE_DERIVE`: When accessing this object, do not access it directly, but make a copy first.


