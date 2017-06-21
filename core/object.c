#include <object.h>
#include <slab.h>
#include <memory.h>

DECLARE_IHTABLE(objtbl, 12);

struct slabcache sc_objs;

__initializer
static void _init_objs(void)
{
	slabcache_init(&sc_objs, sizeof(struct object), NULL, NULL, NULL);
}

static int sz_to_pglevel(size_t sz)
{
	for(int i=0;i<MAX_PGLEVEL;i++) {
		if(sz < mm_page_size(i))
			return i;
	}
	return MAX_PGLEVEL;
}

void obj_create(uint128_t id, size_t maxsz, size_t dataoff)
{
	struct object *obj = slabcache_alloc(&sc_objs);

	obj->id = id;
	obj->maxsz = maxsz;
	obj->pglevel = sz_to_pglevel(maxsz);
	obj->slot = -1;
	obj->dataoff = dataoff;

	ihtable_lock(&objtbl);
	ihtable_insert(&objtbl, &obj->elem, obj->id);
	ihtable_unlock(&objtbl);
}


