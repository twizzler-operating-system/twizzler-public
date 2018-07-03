#pragma once

#include <twz.h>
#include <twzview.h>

long twz_viewcall(long (*target)(), objid_t targetid, objid_t returnid, long a0, long a1, long a2, long a3, long a4, long a5);

static inline void *twz_viewcall_resolve(objid_t target, const char *name)
{
	return (void *)twz_viewcall((long (*)())0x3ffec0002000, target, twz_current_viewid(), (long)name, 0, 0, 0, 0, 0);
}

