#include <twzname.h>
#include <twzsys.h>
#include <twzcore.h>
#include <twzerr.h>
#include <string.h>
#include <stdio.h>

#include <twzkv.h>

static struct object name_index, name_data;

#include <debug.h>
static __attribute__((constructor)) void __init_name(void)
{
}

static int assign_name(struct object *index, struct object *data,
		objid_t id, const char *name)
{
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
	if(resolver != NAME_RESOLVER_DEFAULT)
		return -TE_NOTSUP;
	return assign_name(&name_index, &name_data, id, name);
}

