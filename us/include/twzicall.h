#pragma once
#include <twzviewcall.h>
#include <twzname.h>
#if 0
struct icall {
	objid_t view;
	void *ptr;
};

int twz_icall_init(struct icall *ic, const char *iname, const char *fname)
{
	ic->view = twz_name_resolve(NULL, iname, NAME_RESOLVER_DEFAULT);
	if(ic->view == 0) return -TE_NOENTRY;
	char fn[strlen(fname)+1];
	sprintf(fn, "%s", fname);
	ic->ptr = twz_viewcall_resolve(ic->view, fn);
	if(ic->ptr == NULL) return -TE_NOENTRY;
	return 0;
}

long twz_icall(struct icall *ic, long a0, long a1, long a2, long a3, long a4, long a5)
{
	return twz_viewcall(ic->ptr, ic->view, twz_current_viewid(), a0, a1, a2, a3, a4, a5);
}
#endif

