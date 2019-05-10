#pragma once

#include <stddef.h>
#include <twz/_obj.h>
#define twz_ptr(i, o) (void *)((i)*OBJ_MAXSIZE + (size_t)(o))
