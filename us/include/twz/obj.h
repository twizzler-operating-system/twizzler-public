#pragma once

#include <twz/_obj.h>
#include <twz/_slots.h>

#include <stddef.h>

#define TWZ_OBJ_VALID 1
#define TWZ_OBJ_NORELEASE 2

#define TWZ_OBJ_CACHE 64
#define TWZ_OBJ_CACHE_SIZE 16
typedef struct _twz_object {
	void *_int_base;
	uint64_t _int_flags;
	objid_t _int_id;
	uint32_t _int_vf;
	uint32_t pad;
	uint64_t pad1;
	uint32_t _int_cache[TWZ_OBJ_CACHE_SIZE];
} twzobj;

/* TODO: hide */
#define twz_slot_to_base(s) ({ (void *)((s)*OBJ_MAXSIZE + OBJ_NULLPAGE_SIZE); })
#define twz_base_to_slot(s) ({ ((uintptr_t)(s) / OBJ_MAXSIZE); })

void *twz_object_base(twzobj *);

#define TWZ_OC_HASHDATA MIP_HASHDATA
#define TWZ_OC_DFL_READ MIP_DFL_READ
#define TWZ_OC_DFL_WRITE MIP_DFL_WRITE
#define TWZ_OC_DFL_EXEC MIP_DFL_EXEC
#define TWZ_OC_DFL_USE MIP_DFL_USE
#define TWZ_OC_DFL_DEL MIP_DFL_DEL
#define TWZ_OC_ZERONONCE 0x1000

#define TWZ_OC_VOLATILE 0x2000

int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);

int twz_object_new(twzobj *obj, twzobj *src, twzobj *ku, uint64_t flags);

#define TWZ_KU_USER ((void *)0xfffffffffffffffful)

_Bool objid_parse(const char *name, size_t len, objid_t *id);

/* TODO: hide */
#define twz_ptr_local(p) ({ (typeof(p))((uintptr_t)(p) & (OBJ_MAXSIZE - 1)); })
#define twz_ptr_rebase(fe, p) ({ (typeof(p))((fe)*OBJ_MAXSIZE | (uintptr_t)twz_ptr_local(p)); })

void *__twz_object_lea_foreign(twzobj *o, const void *p);

__attribute__((const)) static inline void *__twz_object_lea(twzobj *o, const void *p)
{
	if(__builtin_expect((uintptr_t)p < OBJ_MAXSIZE, 1)) {
		return (void *)((uintptr_t)o->_int_base + (uintptr_t)p);
	} else {
		void *r = __twz_object_lea_foreign(o, p);
		return r;
	}
}

twzobj twz_object_from_ptr(const void *);

/*
#define TWZ_OBJECT_FROM_PTR(p)                                                                     \
    (twzobj)                                                                                       \
    {                                                                                              \
        .base = (void *)((uintptr_t)p & ~(OBJ_MAXSIZE - 1)), .flags = TWZ_OBJ_VALID,               \
    }
*/

#if 0
/* TODO: hide */
#define TWZ_OBJECT_INIT(s)                                                                         \
	(twzobj)                                                                                       \
	{                                                                                              \
		.base = (void *)(SLOT_TO_VADDR(s)), .flags = TWZ_OBJ_VALID,                                \
	}
#endif

#define twz_object_lea(o, p) ({ (typeof(p)) __twz_object_lea((o), (p)); })

#define twz_ptr_lea(p)                                                                             \
	({                                                                                             \
		twzobj _o = TWZ_OBJECT_FROM_PTR(&(p));                                                     \
		/* TODO: this is tied to TWZ_OBJECT_FROM_PTR... if that call increments refcounts, we'll   \
		 * need to do a release */                                                                 \
		typeof(p) _r = (typeof(p))__twz_object_lea(&_o, (p));                                      \
		_r;                                                                                        \
	});

struct metainfo *twz_object_meta(twzobj *);

int twz_object_init_guid(twzobj *obj, objid_t id, int flags);
int twz_object_init_name(twzobj *obj, const char *name, int flags);

void twz_object_release(twzobj *obj);

int twz_object_tie(twzobj *p, twzobj *c, int flags);
int twz_object_wire_guid(twzobj *view, objid_t id);
int twz_object_tie_guid(objid_t pid, objid_t cid, int flags);
int twz_object_wire(twzobj *, twzobj *);
int twz_object_unwire(twzobj *view, twzobj *obj);
int twz_object_delete(twzobj *obj, int flags);
int twz_object_delete_guid(objid_t id, int flags);
objid_t twz_object_guid(twzobj *o);

void *twz_object_getext(twzobj *obj, uint64_t tag);
int twz_object_addext(twzobj *obj, uint64_t tag, void *ptr);
int twz_object_delext(twzobj *obj, uint64_t tag, void *ptr);

// TODO: audit uses of _store_
#define TWZ_PTR_FLAGS_COPY 0xfffffffffffffffful
int __twz_ptr_store_guid(twzobj *o,
  const void **loc,
  twzobj *target,
  const void *p,
  uint64_t flags);
int __twz_ptr_store_name(twzobj *o,
  const void **loc,
  const char *name,
  const void *p,
  uint64_t flags);
void *__twz_ptr_swizzle(twzobj *o, const void *p, uint64_t flags);

#define twz_ptr_store_guid(o, l, t, p, f)                                                          \
	({                                                                                             \
		typeof(*l) _lt = p;                                                                        \
		__twz_ptr_store_guid(o, (const void **)(l), (t), (p), (f));                                \
	})

#define twz_ptr_store_name(o, l, n, p, f)                                                          \
	({                                                                                             \
		typeof(*l) _lt = p;                                                                        \
		__twz_ptr_store_name(o, (const void **)(l), (n), (p), (f));                                \
	})

#define twz_ptr_swizzle(o, p, f) ({ (typeof(p)) __twz_ptr_swizzle((o), (p), (f)); })

int twz_object_kaction(twzobj *obj, long cmd, ...);
int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags);
int twz_object_ctl(twzobj *obj, int cmd, ...);

static inline struct fotentry *_twz_object_get_fote(twzobj *obj, size_t e)
{
	struct metainfo *mi = twz_object_meta(obj);
	return (struct fotentry *)((char *)mi - sizeof(struct fotentry) * e);
}

#include <sys/types.h>
ssize_t twz_object_addfot(twzobj *obj, objid_t id, uint64_t flags);
