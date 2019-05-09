#pragma once

#define TWZCORE_ALLOCSLOT   1
#define TWZCORE_RELEASESLOT 2
#define TWZCORE_LOOKUPSLOT  3

#include <twz.h>

#define CORECALL_ENTRY_ADDRESS 0x3ffec0001800

static inline long twz_corecall(long num, long a1, long a2, long a3, long a4, long a5)
{
	return ((long (*)(long, long, long, long, long, long))CORECALL_ENTRY_ADDRESS)(num, a1, a2, a3, a4, a5);
}

