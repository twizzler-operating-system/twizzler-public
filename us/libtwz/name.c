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
bool objid_parse(const char *name, size_t len, objid_t *id)
{
	int i;
	*id = 0;
	int shift = 128;

	if(len < 33)
		return false;
	int extra = 0;
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

static struct object nameobj;
static bool __name_init(void)
{
	static const char *nameid = NULL;
	if(!nameid)
		nameid = getenv("TWZNAME");

	if(!nameid)
		return false;

	objid_t id;
	objid_parse(nameid, strlen(nameid), &id);

	twz_object_open(&nameobj, id, FE_READ);

	return true;
}

#if 0
static void copy_names(const char *bsname, struct object *nobj)
{
	struct object bobj;

	objid_t id;
	objid_parse(bsname, strlen(bsname), &id);
	twz_object_open(&bobj, id, FE_READ);
	char *names = twz_obj_base(&bobj);
	while(*names == ',')
		names++;
	while(names && *names) {
		char *eq = strchr(names, '=');
		if(!eq)
			break;
		*eq++ = 0;
		char *next = strchr(eq, ',');
		if(next)
			*next = 0;

		struct btree_val kv = { .mv_data = names, .mv_size = strlen(names) + 1 };

		objid_t thisid;
		objid_parse(eq, strlen(eq), &thisid);
		struct btree_val dv = { .mv_data = &thisid, .mv_size = sizeof(thisid) };

		int r = bt_put(nobj, twz_obj_base(nobj), &kv, &dv, NULL);
		if(r) {
			debug_printf("DUPLICATE NAME (%s)\n", names);
			abort();
		}

		char buf[64];
		snprintf(buf, 64, "#" IDFMT, IDPR(thisid));
		struct btree_val rdv = { .mv_data = names, .mv_size = strlen(names) + 1 };
		struct btree_val rkv = { .mv_data = buf, .mv_size = strlen(buf) + 1 };
		r = bt_put(nobj, twz_obj_base(nobj), &rkv, &rdv, NULL);
		if(r) {
			debug_printf("DUPLICATE NAME\n");
			abort();
		}

		if(!next)
			break;
		names = next + 1;
	}
}
#endif

int twz_name_assign(objid_t id, const char *name)
{
	if(!__name_init())
		return -ENOTSUP;

	char *ch_name = alloca(strlen(name) + 1);
	char *par_name = alloca(strlen(name) + 1);
	char *sl = strrchr(name, '/');

	if(sl) {
		strncpy(par_name, name, sl - par_name);
		strcpy(ch_name, sl + 1);
	} else {
		ch_name = name;
	}

	struct object *parent;
	struct object tmp;
	parent = &nameobj;
	int r;
	if(sl) {
		r = twz_object_open_name(&tmp, par_name, FE_READ | FE_WRITE);
		if(r)
			return r;
		parent = &tmp;
	}

	r = twz_hier_assign_name(parent, ch_name, NAME_ENT_REGULAR, id);
	if(r)
		return r;

	return 0;

#if 0
	struct btree_val kv = { .mv_data = name, .mv_size = strlen(name) + 1 };
	struct btree_val dv = { .mv_data = &id, .mv_size = sizeof(id) };

	int r = bt_put(&nameobj, twz_obj_base(&nameobj), &kv, &dv, NULL);
	if(r)
		return r;

	char buf[64];
	snprintf(buf, 64, "#" IDFMT, IDPR(id));
	struct btree_val rdv = { .mv_data = name, .mv_size = strlen(name) + 1 };
	struct btree_val rkv = { .mv_data = buf, .mv_size = strlen(buf) + 1 };
	bt_put(&nameobj, twz_obj_base(&nameobj), &rkv, &rdv, NULL);
	return 0;
#endif
}

int __name_bootstrap(void)
{
	const char *bsname = getenv("BSNAME");
	if(!bsname)
		return -ENOENT;

	setenv("TWZNAME", bsname, 1);
	return 0;

#if 0
	int r;
	objid_t id;
	/* TODO: make this read-only */
	r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &id);
	if(r < 0)
		return r;

	struct object no;
	twz_object_open(&no, id, FE_READ | FE_WRITE);
	bt_init(&no, twz_obj_base(&no));

	char tmp[64];
	snprintf(tmp, 64, IDFMT, IDPR(id));

	copy_names(bsname, &no);
	if(setenv("TWZNAME", tmp, 1) == -1)
		return -EGENERIC;
	return 0;
#endif
}

#if 0
static int __twz_name_dfl_reverse(objid_t id, char *name, size_t *nl, int flags)
{
	if(!__name_init())
		return -ENOTSUP;
	char buf[64];
	snprintf(buf, 64, "#" IDFMT, IDPR(id));
	struct btree_val kv = { .mv_data = buf, .mv_size = strlen(buf) + 1 };
	struct btree_val dv;

	struct btree_node *n = bt_lookup(&nameobj, twz_obj_base(&nameobj), &kv);
	if(!n)
		return -ENOENT;
	bt_node_get(&nameobj, twz_obj_base(&nameobj), n, &dv);

	if(*nl >= dv.mv_size) {
		memcpy(name, dv.mv_data, dv.mv_size);
		return 0;
	} else {
		*nl = dv.mv_size;
		return -ENOSPC;
	}
}
#endif

static int __twz_name_dfl_resolve(struct object *obj, const char *name, int flags, objid_t *id)
{
	if(!__name_init())
		return -ENOTSUP;

	struct twz_name_ent ent;
	int r = twz_hier_resolve_name(&nameobj, name, 0, &ent);
	if(r)
		return r;
	*id = ent.id;
	return 0;

#if 0

	if(name[0] == '.' && name[1] == '/')
		name += 2;
	const char *vname = obj ? twz_ptr_lea(obj, name) : name;

	struct btree_val kv = { .mv_data = vname, .mv_size = strlen(vname) + 1 };
	struct btree_val dv;

	struct btree_node *n = bt_lookup(&nameobj, twz_obj_base(&nameobj), &kv);
	if(!n) {
		static int unix_root = 0;
		static struct object ur;
		if(!unix_root) {
			unix_root = 1;
			if(twz_object_open_name(&ur, "__unix__root__", FE_READ)) {
				unix_root = -1;
				return -ENOENT;
			}
		}
		struct twz_name_ent ent;
		int r = twz_hier_resolve_name(&ur, name, 0, &ent);
		if(r)
			return r;
		*id = ent.id;
		return 0;
	}
	bt_node_get(&nameobj, twz_obj_base(&nameobj), n, &dv);

	if(id)
		*id = *(objid_t *)dv.mv_data;

	return 0;
#endif
}

ssize_t twz_name_dfl_getnames(const char *startname, struct twz_nament *ents, size_t len)
{
	return -ENOTSUP;
#if 0
	if(!__name_init())
		return -ENOTSUP;
	struct btree_node *n;
	if(startname) {
		struct btree_val kv = { .mv_data = startname, .mv_size = strlen(startname) + 1 };
		struct btree_val dv;

		n = bt_lookup(&nameobj, twz_obj_base(&nameobj), &kv);
		if(!n)
			return -ENOENT;
		n = bt_next(&nameobj, twz_obj_base(&nameobj), n);
	} else {
		n = bt_first(&nameobj, twz_obj_base(&nameobj));
	}

	size_t recs = 0;
	for(; n; n = bt_next(&nameobj, twz_obj_base(&nameobj), n)) {
		struct btree_val dv;
		struct btree_val kv;
		bt_node_get(&nameobj, twz_obj_base(&nameobj), n, &dv);
		bt_node_getkey(&nameobj, twz_obj_base(&nameobj), n, &kv);
		if(*(char *)kv.mv_data == '#') {
			/* name for reverse lookup. Skip */
			continue;
		}
		size_t rl = sizeof(struct twz_nament) + kv.mv_size;
		rl = ((rl - 1) & ~(_Alignof(struct twz_nament) - 1)) + _Alignof(struct twz_nament);
		if(rl > len && !recs)
			return -EINVAL;
		else if(rl > len) {
			return recs;
		}

		ents->id = *(objid_t *)dv.mv_data;
		ents->reclen = rl;
		ents->flags = 0;
		memcpy(ents->name, kv.mv_data, kv.mv_size);
		recs++;

		ents = (struct twz_nament *)((uintptr_t)ents + rl);
		len -= rl;
	}

	return recs;
#endif
}

int twz_name_resolve(struct object *obj,
  const char *name,
  int (*fn)(struct object *, const char *, int, objid_t *),
  int flags,
  objid_t *id)
{
	if(obj)
		fn = twz_ptr_lea(obj, fn);
	if(fn)
		return fn(obj, name, flags, id);
	return __twz_name_dfl_resolve(obj, name, flags, id);
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
	// return __twz_name_dfl_reverse(id, name, nl, flags);
}
