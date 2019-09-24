#pragma once

#include <twz/_obj.h>
#include <twz/_slots.h>

#include <stddef.h>

struct object {
	void *base;
	uint64_t flags;
	objid_t id;
};

#define twz_slot_to_base(s) ({ (void *)((s)*OBJ_MAXSIZE + OBJ_NULLPAGE_SIZE); })
#define twz_obj_base(s) ({ (void *)((uintptr_t)((s)->base) + OBJ_NULLPAGE_SIZE); })

#define TWZ_OC_HASHDATA MIP_HASHDATA
#define TWZ_OC_DFL_READ MIP_DFL_READ
#define TWZ_OC_DFL_WRITE MIP_DFL_WRITE
#define TWZ_OC_DFL_EXEC MIP_DFL_EXEC
#define TWZ_OC_DFL_USE MIP_DFL_USE
#define TWZ_OC_DFL_DEL MIP_DFL_DEL
#define TWZ_OC_ZERONONCE 0x1000

int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);
_Bool objid_parse(const char *name, size_t len, objid_t *id);

void *__twz_ptr_lea_foreign(const struct object *o, const void *p);

static inline void *__twz_ptr_lea(const struct object *o, const void *p)
{
	if((uintptr_t)p < OBJ_MAXSIZE) {
		return (void *)((uintptr_t)o->base + (uintptr_t)p);
	} else {
		return __twz_ptr_lea_foreign(o, p);
	}
}

#define TWZ_OBJECT_FROM_PTR(p)                                                                     \
	(struct object)                                                                                \
	{                                                                                              \
		.base = (void *)((uintptr_t)p & ~(OBJ_MAXSIZE - 1))                                        \
	}

#define TWZ_OBJECT_INIT(s)                                                                         \
	(struct object)                                                                                \
	{                                                                                              \
		.base = (void *)(SLOT_TO_VADDR(s)),                                                        \
	}

#define twz_ptr_lea(o, p) ({ (typeof(p)) __twz_ptr_lea((o), (p)); })

#define twz_ptr_local(p) ({ (typeof(p))((uintptr_t)(p) & (OBJ_MAXSIZE - 1)); })

#define twz_object_meta(o)                                                                         \
	({ (struct metainfo *)(((char *)(o)->base + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE)); })

int twz_object_open(struct object *obj, objid_t id, int flags);
int twz_object_open_name(struct object *obj, const char *name, int flags);

objid_t twz_object_id(struct object *o);
#define SLOT_TO_VADDR(s) ({ (void *)((s)*OBJ_MAXSIZE); })
#define VADDR_TO_SLOT(s) ({ (size_t)((uintptr_t)(s) / OBJ_MAXSIZE); })

void *twz_object_getext(struct object *obj, uint64_t tag);
int twz_object_addext(struct object *obj, uint64_t tag, void *ptr);

#define twz_ptr_store(o, p, f, res) ({ __twz_ptr_store((o), (p), (f), (const void **)(res)); })

int __twz_ptr_store(struct object *o, const void *p, uint32_t flags, const void **res);

#define twz_ptr_rebase(fe, p) ({ (typeof(p))((fe)*OBJ_MAXSIZE | (uintptr_t)twz_ptr_local(p)); })

int __twz_ptr_make(struct object *obj, objid_t id, const void *p, uint32_t flags, const void **res);

#define twz_ptr_make(o, id, p, f, res)                                                             \
	({ __twz_ptr_make((o), (id), (p), (f), (const void **)(res)); })
