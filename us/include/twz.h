#pragma once

#include <stdint.h>

typedef unsigned __int128 objid_t;
typedef signed long ssize_t;

#define IDFMT "%16.16lx:%16.16lx"
#define IDPR(id) (uint64_t)((id) >> 64), (uint64_t)(id)
#define ID_LO(d) ({ (uint64_t)(d); })
#define ID_HI(d) ({ (uint64_t)((d) >> 64); })
#define MKID(hi,lo) ((((objid_t)(hi)) << 64) | (lo))

#undef __unused
#define __unused __attribute__((unused))

