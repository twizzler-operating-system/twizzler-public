Structured Objects
==================

Most objects have a specific purpose, with a specific layout. Often, the object contains a _header_,
usually located at the base of the data region of the object. An API over an object typically either
assumes that the object has such a given layout, in which case the API might look like:

``` {.c}
foo_action(twzobj *obj, ...);
```

However, this is inflexible. An object might contain
sub-components that also have an API and a header. For example, many objects implement the events
API (see Events API) as part of a larger structure (byte-streams, for example, are objects with a
structure and thus a header struct in the base, but they _also_ implement the events API which has
its own header structure). Many APIs also provide variants of functions that do _not_ assume their
header struct is at the base. Such an API might provide a variant of the above function:

``` {.c}
foo_action_hdr(twzobj *obj, struct foo_hdr *hdr, ...);
```

This allows an object to implement multiple APIs. The programmer must locate the appropriate header
before passing it, of course. Often, within an object's API, this is straight-forward. For example,
a header might look as follows:
``` {.c}
struct bstream_hdr {
	...
	struct event_hdr evh;
	...
};
```
Then, within the bstream API, we can easily pass the event header to the event API functions we need
to call.

However, not all headers can be located this way. An object might provide multiple APIs that _don't_
depend on each other. For this, Twizzler provides functions to assist locating header structs, and
registering and deregistering header structs. Each API that has a header that can be located this
way with a _tag_. Each API that can be located this way provides a unique tag.

## twz_object_getext

``` {.c}
#include <twz/obj.h>
void *twz_object_getext(twzobj *obj, uint64_t tag);
```

Locate a structured object header within an object `obj` using `tag`, returning a d-ptr to the
header structure associated with tag `tag`.

### Return Value
Returns a pointer to the located header struct on success, NULL on failure. This function returns no
errors, only returning NULL if the header struct was not found.

## twz_object_addext

``` {.c}
#include <twz/obj.h>
int twz_object_addext(twzobj *obj, uint64_t tag, void *ptr);
```

Add a pointer to a structured header to the list of headers that can be located with
`twz_object_getext`. The pointer (`ptr`) must be a local pointer to object `obj`.

### Return Value
Returns 0 on success, error code on error.

### Errors

* `-EEXIST`: The tag already exists in the tag list.
* `-ENOMEM`: Not enough memory to add tag.

## twz_object_delext

``` {.c}
#include <twz/obj.h>
int twz_object_delext(twzobj *obj, uint64_t tag);
```

Remove a tag from the object's tag list.

### Return Value
Returns 0 on success, error code on error.

### Errors
* `-ENOENT`: `tag` was not found in the tag list.

