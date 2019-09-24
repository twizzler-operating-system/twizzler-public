#include <stdio.h>
#include <twz/_err.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/view.h>
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	if(flags & TWZ_OC_ZERONONCE) {
		flags = (flags & ~TWZ_OC_ZERONONCE) | TWZ_SYS_OC_ZERONONCE;
	}
	return sys_ocreate(flags, kuid, src, id);
}

int twz_object_open(struct object *obj, objid_t id, int flags)
{
	ssize_t slot = twz_view_allocate_slot(NULL, id, flags);
	if(slot < 0)
		return slot;

	obj->base = (void *)(OBJ_MAXSIZE * (slot));
	obj->id = id;
	obj->flags = 0;
	// debug_printf("opened " IDFMT " -> %p\n", IDPR(id), obj->base);
	return 0;
}

objid_t twz_object_id(struct object *o)
{
	if(o->id)
		return o->id;
	objid_t id = 0;
	if(twz_vaddr_to_obj(o->base, &id, NULL)) {
		/* TODO: raise fault */
	}
	return (o->id = id);
}

int twz_object_open_name(struct object *obj, const char *name, int flags)
{
	objid_t id;
	int r = twz_name_resolve(NULL, name, NULL, 0, &id);
	if(r < 0)
		return r;
	ssize_t slot = twz_view_allocate_slot(NULL, id, flags);
	if(slot < 0)
		return slot;

	obj->base = (void *)(OBJ_MAXSIZE * (slot));
	obj->id = id;
	obj->flags = 0;
	// debug_printf("opened name %s : " IDFMT " -> %p\n", name, IDPR(id), obj->base);
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

int twz_object_addext(struct object *obj, uint64_t tag, void *ptr)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct metaext *e = &mi->exts[0];

	while((char *)e < (char *)mi + mi->milen) {
		if(e->tag == 0) {
			e->ptr = twz_ptr_local(ptr);
			e->tag = tag;
			return 0;
		}
		e++;
	}
	return -ENOSPC;
}

ssize_t twz_object_addfot(struct object *obj, objid_t id, uint64_t flags)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct fotentry *fe = (void *)((char *)mi + mi->milen);
	/* TODO: large FOTs */
	for(size_t e = 1; e < 64; e++) {
		if(fe[e].id == id && fe[e].flags == flags)
			return e;
		if(fe[e].id == 0) {
			fe[e].id = id;
			fe[e].flags = flags;
			if(mi->fotentries <= e)
				mi->fotentries = e + 1;
			return e;
		}
	}
	return -ENOSPC;
}

int __twz_ptr_make(struct object *obj, objid_t id, const void *p, uint32_t flags, const void **res)
{
	ssize_t fe = twz_object_addfot(obj, id, flags);
	if(fe < 0)
		return fe;

	*res = twz_ptr_rebase(fe, p);

	return 0;
}

int __twz_ptr_store(struct object *obj, const void *p, uint32_t flags, const void **res)
{
	objid_t target;
	int r = twz_vaddr_to_obj(p, &target, NULL);
	if(r)
		return r;

	return __twz_ptr_make(obj, target, p, flags, res);
}

void *__twz_ptr_lea_foreign(const struct object *o, const void *p)
{
	struct metainfo *mi = twz_object_meta(o);
	struct fotentry *fe = (void *)((char *)mi + mi->milen);

	int r;
	size_t slot = (uintptr_t)p / OBJ_MAXSIZE;
	if(slot >= mi->fotentries)
		return NULL;

	if(fe[slot].id == 0)
		return NULL;

	objid_t id;
	if(fe[slot].flags & FE_NAME) {
		r = twz_name_resolve(o, fe[slot].name.data, fe[slot].name.nresolver, 0, &id);
		if(r)
			return NULL;
	} else {
		id = fe[slot].id;
	}

	ssize_t ns = twz_view_allocate_slot(NULL, id, fe[slot].flags & (FE_READ | FE_WRITE | FE_EXEC));
	if(ns < 0)
		return NULL;

	// debug_printf("---> %p : " IDFMT "\n", p, IDPR(id));
	// if(twz_view_set(NULL, ns, id, fe[slot].flags & (FE_READ | FE_WRITE | FE_EXEC)))
	//	return NULL;
	return twz_ptr_rebase(ns, (void *)p);
}
