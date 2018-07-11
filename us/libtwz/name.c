#include <twzname.h>
#include <twzsys.h>
#include <twzcore.h>
#include <twzerr.h>
#include <string.h>
#include <stdio.h>

#include <twzkv.h>

static bool name_init = false, name_data_init = false;
static struct object name_index, name_data;
static inline bool objid_parse(const char *name, objid_t *id)
{
	int i;
	*id = 0;
	int shift = 128;

	for(i=0;i<33;i++) {
		char c = *(name + i);
		if(c == ':' && i == 16) {
			continue;
		}
		if(!((c >= '0' && c <= '9')
					|| (c >= 'a' && c <= 'f')
					|| (c >= 'A' && c <= 'F'))) {
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
	return i == 33;
}

static void __name_data_init(void)
{
	twzkv_create_data(&name_data);
}

static void __name_init(void)
{
	struct object kc;
	if(twz_object_open(&kc, 1, FE_READ) < 0) {
		return;
	}

	char *kcdata = twz_ptr_base(&kc);
	char *r = strstr(kcdata, "name=");
	if(!r) {
		return;
	}

	char *idstr = r + 5;
	objid_t id = 0;
	if(!objid_parse(idstr, &id)) {
		return;
	}

	twz_object_open(&name_index, id, FE_READ | FE_WRITE);

	name_init = true;
}

static int assign_name(struct object *index, struct object *data,
		objid_t id, const char *name)
{
	if(!name_init) return -1;
	struct twzkv_item key = {
		.data = (char *)name,
		.length = strlen(name),
	};
	struct twzkv_item value = {
		.data = &id,
		.length = sizeof(id),
	};

	return twzkv_put(index, data, &key, &value);
}

static objid_t resolve_name(struct object *index, const char *name)
{
	if(!name_init) return 0;
	struct twzkv_item key = {
		.data = (char *)name,
		.length = strlen(name),
	};
	struct twzkv_item value;

	if(twzkv_get(index, &key, &value)) {
		return 0;
	}
	return *(objid_t *)value.data;
}

objid_t twz_name_resolve(struct object *o, const char *name, uint64_t resolver)
{
	if(!name_init) {
		__name_init();
	}
	if(resolver != NAME_RESOLVER_DEFAULT)
		return 0;
	objid_t id = resolve_name(&name_index, name);
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
	if(!name_init) {
		__name_init();
	}
	if(!name_data_init) {
		__name_data_init();
	}
	if(resolver != NAME_RESOLVER_DEFAULT)
		return -TE_NOTSUP;
	return assign_name(&name_index, &name_data, id, name);
}

