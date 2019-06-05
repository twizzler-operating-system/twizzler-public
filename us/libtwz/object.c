#include <twz/obj.h>
#include <twz/sys.h>
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	return sys_ocreate(flags, kuid, src, id);
}
