#include <libgen.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/_err.h>
#include <twz/btree.h>
#include <twz/hier.h>
#include <twz/name.h>
#include <twz/obj.h>

#include <twz/debug.h>
/* TODO: make a real function for this */
bool objid_parse(const char *name, size_t len, objid_t *id)
{
	size_t i;
	*id = 0;
	int shift = 128;

	if(len < 33)
		return false;
	size_t extra = 0;
	for(i = 0; i < 32 + extra && i < len; i++) {
		char c = *(name + i);
		if(c == ':') {
			extra++;
			continue;
		}
		if(c == '0' && *(name + i + 1) == 'x') {
			i++;
			extra += 2;
			continue;
		}
		if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
			break;
		}
		if(c >= 'A' && c <= 'F') {
			c += 'a' - 'A';
		}

		objid_t this = 0;
		if(c >= 'a' && c <= 'f') {
			this = c - 'a' + 0xa;
		} else {
			this = c - '0';
		}

		shift -= 4;
		*id |= this << shift;
	}
	/* finished parsing? */
	return i >= 33;
}

static twzobj nameobj;
static const char *nameid = NULL;
static bool __name_init(void)
{
	if(!nameid)
		nameid = getenv("TWZNAME");

	if(!nameid)
		return false;

	objid_t id;
	objid_parse(nameid, strlen(nameid), &id);

	twz_object_init_guid(&nameobj, id, FE_READ);

	return true;
}

int twz_name_switch_root(twzobj *obj)
{
	__name_init();
	twz_object_release(&nameobj);

	char name[128];
	sprintf(name, IDFMT, IDPR(twz_object_guid(obj)));
	setenv("TWZNAME", name, 1);
	nameid = getenv("TWZNAME");

	twz_object_init_guid(&nameobj, twz_object_guid(obj), FE_READ);

	return 0;
}

int twz_name_assign_namespace(objid_t id, const char *name)
{
	if(!__name_init())
		return -ENOTSUP;

	char *d1 = alloca(strlen(name) + 1);
	char *d2 = alloca(strlen(name) + 1);

	strcpy(d1, name);
	strcpy(d2, name);

	int r;
	char *par_name = dirname(d1);
	char *ch_name = basename(d2);
	twzobj parent;
	r = twz_object_init_name(&parent, par_name, FE_READ | FE_WRITE);
	if(r)
		return r;

	r = twz_hier_assign_name(&parent, ch_name, NAME_ENT_NAMESPACE, id);
	if(r)
		return r;

	return 0;
}

int twz_name_assign(objid_t id, const char *name)
{
	if(!__name_init())
		return -ENOTSUP;

	char *d1 = alloca(strlen(name) + 1);
	char *d2 = alloca(strlen(name) + 1);

	strcpy(d1, name);
	strcpy(d2, name);

	int r;
	char *par_name = dirname(d1);
	char *ch_name = basename(d2);
	twzobj parent;
	r = twz_object_init_name(&parent, par_name, FE_READ | FE_WRITE);
	if(r)
		return r;

	r = twz_hier_assign_name(&parent, ch_name, NAME_ENT_REGULAR, id);
	if(r)
		return r;

	return 0;
}

int __name_bootstrap(void)
{
	const char *bsname = getenv("BSNAME");
	if(!bsname)
		return -ENOENT;

	setenv("TWZNAME", bsname, 1);
	return 0;
}

twzobj *twz_name_get_root(void)
{
	if(!__name_init())
		abort();

	return &nameobj;
}

static int __twz_name_dfl_resolve(twzobj *obj, const char *name, int flags, objid_t *id)
{
	(void)obj;
	(void)flags;
	if(!__name_init())
		return -ENOTSUP;

	struct twz_name_ent ent;
	int r = twz_hier_resolve_name(&nameobj, name, 0, &ent);
	if(r)
		return r;
	*id = ent.id;
	return ent.id ? 0 : -ENOENT;
}

ssize_t twz_name_dfl_getnames(const char *startname, struct twz_nament *ents, size_t len)
{
	(void)startname;
	(void)ents;
	(void)len;
	return -ENOTSUP;
}

int twz_name_resolve(twzobj *obj,
  const char *name,
  int (*fn)(twzobj *, const char *, int, objid_t *),
  int flags,
  objid_t *id)
{
	if(obj)
		fn = twz_object_lea(obj, fn);
	if(fn)
		return fn(obj, name, flags, id);
	return __twz_name_dfl_resolve(obj, name, flags, id);
}

#include <twz/fault.h>
#include <twz/view.h>
static int __twz_fot_indirect_resolve_dfl(twzobj *obj,
  struct fotentry *fe,
  const void *p,
  void **vptr,
  uint64_t *info)
{
	int r;
	objid_t id;
	r = twz_name_resolve(obj, fe->name.data, fe->name.nresolver, 0, &id);
	if(r) {
		*info = FAULT_PPTR_RESOLVE;
		return r;
	}

	ssize_t ns = twz_view_allocate_slot(NULL, id, fe->flags & (FE_READ | FE_WRITE | FE_EXEC));
	if(ns < 0) {
		*info = FAULT_PPTR_RESOURCES;
		return ns;
	}

	void *_r = twz_ptr_rebase(ns, (void *)p);
	size_t slot = VADDR_TO_SLOT(p);
	if(slot < TWZ_OBJ_CACHE_SIZE) {
		obj->_int_cache[slot] = ns;
	}
	*vptr = _r;
	return 0;
}

#include <dlfcn.h>

#include <twz/thread.h>
static int __twz_fot_indirect_resolve_sofn(twzobj *obj,
  struct fotentry *fe,
  const void *p,
  void **vptr,
  uint64_t *info)
{
	size_t slot = VADDR_TO_SLOT(p);
	if(obj->_int_sofn_cache[slot]) {
		*vptr = obj->_int_sofn_cache[slot];
		return 0;
	}

	char *name = twz_object_lea(obj, fe->name.data);

	size_t sl = strlen(name);
	if(sl > 128) {
		*info = FAULT_PPTR_RESOURCES;
		return -EINVAL;
	}

	char *dc = strstr(name, "::");
	char tmpname[sl + 1];
	char *symname = NULL;
	if(dc) {
		strcpy(tmpname, name);
		name = tmpname;
		char *dc2 = strstr(tmpname, "::");
		*dc2 = 0;
		symname = dc + 2;
	}

	void *dl = dlopen(name, RTLD_NOW);
	if(!dl) {
		*info = FAULT_PPTR_RESOLVE;
		return -errno;
	}

	void *sym;
	if(symname) {
		sym = dlsym(dl, symname);
		if(!sym) {
			*info = FAULT_PPTR_RESOLVE;
			return -errno;
		}
	} else {
		/* TODO: get base address of the DL */
		*info = FAULT_PPTR_RESOLVE;
		return -ENOTSUP;
	}

	*vptr = sym;
	obj->_int_sofn_cache[slot] = sym;

	return 0;
}

int twz_fot_indirect_resolve(twzobj *obj,
  struct fotentry *fe,
  const void *p,
  void **vptr,
  uint64_t *info)
{
	switch((long)fe->name.nresolver) {
		case(long)TWZ_NAME_RESOLVER_SOFN:
			return __twz_fot_indirect_resolve_sofn(obj, fe, p, vptr, info);
			break;
	}
	if(fe->name.nresolver) {
		int (*fn)(twzobj *, struct fotentry * fe, const void *, void **, uint64_t *) =
		  twz_object_lea(obj, fe->name.nresolver);
		return fn(obj, fe, p, vptr, info);
	}
	return __twz_fot_indirect_resolve_dfl(obj, fe, p, vptr, info);
}

int twz_name_reverse_lookup(objid_t id,
  char *name,
  size_t *nl,
  ssize_t (*fn)(objid_t id, char *name, size_t *nl, int flags),
  int flags)
{
	if(fn)
		return fn(id, name, nl, flags);
	return -ENOTSUP;
}
