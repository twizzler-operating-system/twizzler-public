#include <twz/_err.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/view.h>
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	return sys_ocreate(flags, kuid, src, id);
}

int twz_object_open(struct object *obj, objid_t id, int flags)
{
	static int i = 0;
	int x = i++;

	twz_view_set(NULL, 0x100 + x, id, FE_READ | FE_WRITE); // TODO

	obj->base = (void *)(OBJ_MAXSIZE * (x + 0x100));
	return 0;
}

void *twz_object_getext(struct object *obj, uint64_t tag)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct metaext *e = &mi->exts[0];

	while((char *)e < (char *)mi + mi->milen) {
		if(e->tag == tag) {
			return twz_ptr_lea(obj, e->ptr);
		}
		e++;
	}
	return NULL;
}

#include <twz/debug.h>
int twz_object_addext(struct object *obj, uint64_t tag, void *ptr)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct metaext *e = &mi->exts[0];

	while((char *)e < (char *)mi + mi->milen) {
		debug_printf("-> %p %lx\n", e, e->tag);
		if(e->tag == 0) {
			e->ptr = twz_ptr_local(ptr);
			e->tag = tag;
			return 0;
		}
		e++;
	}
	return -ENOSPC;
}
