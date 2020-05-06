Object IDs (doc/us/object_ids.md)
=================================

All objects in the system have a 128 bit GUID (or ID). These IDs are globally unique, and are
randomly generated. The header `twz/obj.h` provides the type `objid_t` which can store object IDs.

Typically it is preferred for applications to use object handles (see Object API) instead of IDs
directly.

## Printing object IDs

Twizzler provides an object ID format specifier for printf-like functions: `IDFMT` and `IDPR(objid_t id)`.
They are used as follows:
``` {.c}
objid_t id = /* something */;
printf("Here is the ID: " IDFMT "\n", IDPR(id));
```
