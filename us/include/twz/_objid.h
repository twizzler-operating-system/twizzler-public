#pragma once
#define IDFMT "%.16lx:%.16lx"
#define IDPR(x) (uint64_t)(x >> 64), (uint64_t)(x & 0xffffffffffffffff)

typedef unsigned __int128 objid_t;

#define MKID(hi, lo) ({ (((objid_t)(hi)) << 64) | (objid_t)(lo); })

#define ID_LO(id) ({ (uint64_t) id; })
#define ID_HI(id) ({ (uint64_t)(id >> 64); })
