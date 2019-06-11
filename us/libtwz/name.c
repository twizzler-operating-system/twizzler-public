#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/_err.h>
#include <twz/btree.h>
#include <twz/obj.h>

#include <twz/debug.h>
static bool objid_parse(const char *name, size_t len, objid_t *id)
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

		bt_put(nobj, twz_obj_base(nobj), &kv, &dv, NULL);

		if(!next)
			break;
		names = next + 1;
	}
}

int __name_bootstrap(void)
{
	const char *bsname = getenv("BSNAME");
	if(!bsname)
		return -ENOENT;

	int r;
	objid_t id;
	r = twz_object_create(TWZ_OC_DFL_READ, 0, 0, &id);
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
}

static int __twz_name_dfl_resolve(struct object *obj, const char *name, int flags, objid_t *id)
{
	if(!__name_init())
		return -ENOTSUP;

	const char *vname = obj ? twz_ptr_lea(obj, name) : name;

	struct btree_val kv = { .mv_data = vname, .mv_size = strlen(vname) + 1 };
	struct btree_val dv;

	struct btree_node *n = bt_lookup(&nameobj, twz_obj_base(&nameobj), &kv);
	if(!n)
		return -ENOENT;
	bt_node_get(&nameobj, twz_obj_base(&nameobj), n, &dv);

	if(id)
		*id = *(objid_t *)dv.mv_data;

	return 0;
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
