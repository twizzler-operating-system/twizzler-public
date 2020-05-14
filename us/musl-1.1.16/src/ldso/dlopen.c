#include "libc.h"
#include <dlfcn.h>

__attribute__((__visibility__("hidden"))) void __dl_seterr(const char *, ...);

#include "../../../../../../us/include/twz/debug.h"
static void *stub_dlopen(const char *file, int mode)
{
	__dl_seterr("Dynamic loading not supported");
	return 0;
}

weak_alias(stub_dlopen, dlopen);
