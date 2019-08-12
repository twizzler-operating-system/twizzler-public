Object IDs (us/doc/object_ids.md)
=================================

Twizzler provides basic manipulation routines for object IDs. An object ID is 128 bits, and is
stored in the type `objid_t`.

## Printing object IDs

Twizzler provides an object ID format specifier for printf-like functions: `IDFMT` and `IDPR(objid_t id)`.
They are used as follows:
``` {.c}
printf("Here is the ID: " IDFMT "\n", IDPR(id));
```
