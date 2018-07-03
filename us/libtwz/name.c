#include <twzname.h>
#include <twzsys.h>
#include <twzcore.h>
#include <twzerr.h>
#include <string.h>
#include <stdio.h>

static int assign_name(objid_t id __unused, const char *name __unused)
{
#if 0
	char target[128];
	char ln[128];
	snprintf(target, 128, "../id/%16.16lx:%16.16lx",
			(uint64_t)(id >> 64), (uint64_t)id);
	snprintf(ln, 128, "name/%s", name);
	if(fbsd_symlink(target, ln) < 0) {
		return -1;
	}
#endif
	return 0;
}

static objid_t resolve_name(const char *name __unused)
{
#if 0
	static const int rev[] = {
		['0'] = 0,
		['1'] = 1,
		['2'] = 2,
		['3'] = 3,
		['4'] = 4,
		['5'] = 5,
		['6'] = 6,
		['7'] = 7,
		['8'] = 8,
		['9'] = 9,
		['a'] = 0xa,
		['b'] = 0xb,
		['c'] = 0xc,
		['d'] = 0xd,
		['e'] = 0xe,
		['f'] = 0xf,
	};
	char tmp[128];
	memset(tmp, 0, 128);
	char path[128];
	snprintf(path, 128, "name/%s", name);
	if(fbsd_readlink(path, tmp, sizeof(tmp)) < 0) {
		//printf("Name %s cannot be resolved\n", name);
		return 0;
	}
	char *ptr = tmp + 6;
	int shift = 128;
	objid_t ret = 0;
	while(*ptr) {
		if((*ptr >= '0' && *ptr <= '9')
				|| (*ptr >= 'a' && *ptr <= 'f')) {
			unsigned __int128 val = rev[(int)*ptr];
			shift -= 4;
			ret |= val << shift;
		}
		ptr++;
	}
	if(shift) {
		//printf("Resolving name %s has invalid target (%d)\n", name, shift);
		return 0;
	}
	//printf("Resolved name %s to %16.16lx:%16.16lx\n", name,
	//		(uint64_t)(ret >> 64), (uint64_t)ret);
	return ret;
#endif
	return 0;
}

objid_t twz_name_resolve(struct object *o, const char *name, uint64_t resolver)
{
	if(resolver != NAME_RESOLVER_DEFAULT)
		return 0;
	objid_t id = resolve_name(name);
	if(id == 0) {
		return 0;
	}
	if(o) {
		twz_object_open(o, id, 0);
	}
	return id;
}

int twz_name_assign(objid_t id, const char *name, uint64_t resolver)
{
	if(resolver != NAME_RESOLVER_DEFAULT)
		return -TE_NOTSUP;
	return assign_name(id, name);
}

