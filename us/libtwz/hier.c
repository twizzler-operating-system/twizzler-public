#include <errno.h>
#include <string.h>
#include <twz/debug.h>
#include <twz/hier.h>
#include <twz/obj.h>

static struct twz_name_ent *__get_name_ent(struct object *ns, const char *path, size_t plen)
{
	/* note that the dlen field includes the null terminator */
	struct twz_namespace_hdr *hdr = twz_obj_base(ns);
	if(hdr->magic != NAMESPACE_MAGIC) {
		return NULL;
	}
	struct twz_name_ent *ent = hdr->ents;

	while(ent->dlen) {
		if((ent->flags & NAME_ENT_VALID) && ent->dlen >= plen + 1) {
			if(!memcmp(ent->name, path, plen) && ent->name[plen + 1] == 0) {
				/* found! */
				return ent;
			}
		}
		size_t reclen = sizeof(*ent) + ent->dlen;
		reclen = (reclen + 15) & ~15;
		ent = (struct twz_name_ent *)((char *)ent + reclen);
	}
	return NULL;
}

int twz_hier_assign_name(struct object *ns, const char *name, int type)
{
	struct twz_namespace_hdr *hdr = twz_obj_base(ns);
	if(hdr->magic != NAMESPACE_MAGIC) {
		return -EINVAL;
	}
	struct twz_name_ent *ent = hdr->ents;

	size_t len = strlen(name) + 1;
	while(ent->dlen) {
		if(!(ent->flags & NAME_ENT_VALID) && ent->dlen >= len) {
			ent->flags |= NAME_ENT_VALID;
			ent->type = type;
			ent->resv0 = 0;
			ent->resv1 = 0;
			strcpy(ent->name, name);
		}
		size_t reclen = sizeof(*ent) + ent->dlen;
		reclen = (reclen + 15) & ~15;
		ent = (struct twz_name_ent *)((char *)ent + reclen);
	}
	ent->dlen = (len + 15) & ~15;
	ent->flags = NAME_ENT_VALID;
	ent->type = type;
	ent->resv0 = 0;
	ent->resv1 = 0;
	strcpy(ent->name, name);
	return 0;
}

/* resolve a path starting from namespace ns. Note that this does _not_ operate
 * like a unix path resolver in some ways:
 *   - a first element '/' character does not differ from a path without one. That is,
 *     /usr and usr are both treated the same way here. In this sense, 'ns' is more like the
 *     root of the unix path system.
 */
int twz_hier_resolve_name(struct object *ns, const char *path, int flags, struct twz_name_ent *ent)
{
	while(*path == '/')
		path++;
	if(!*path) {
		/* no path to traverse; return 0, with an ID of 0, and the caller can figure it out */
		ent->id = 0;
		return 0;
	}

	/* consume the next part of the path, and recurse if necessary. */
	char *ndl = strchr(path, '/');
	size_t elen = ndl ? (size_t)(ndl - path) : strlen(path);
	struct twz_name_ent *ne = __get_name_ent(ns, path, elen);
	if(!ne) {
		return -ENOENT;
	}
	if(ndl) {
		/* not the last element of the path. Note: if the path has the form '/usr/bin/' then
		 * we actually _are_ the last element, but have no way of knowing yet. That's okay, though,
		 * we'll handle that. Firstly, a path lookup of a _file_ will fail if we append a /, so we
		 * can check that now */
		if(ne->type != NAME_ENT_NAMESPACE) {
			return -ENOTDIR;
		}
		struct object next;
		twz_object_open(&next, ne->id, FE_READ);
		int r = twz_hier_resolve_name(&next, ndl + 1, flags, ent);
		if(ent->id || r) {
			return r;
		}
	}
	*ent = *ne;
	ent->dlen = 0;
	return 0;
}
