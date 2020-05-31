#pragma once

#include <twz/__twz.h>

#include <twz/_obj.h>
#include <twz/_slots.h>

#include <stddef.h>

#define TWZ_OBJ_CACHE_SIZE 16
typedef struct _twz_object {
	void *_int_base;
	uint64_t _int_flags;
	objid_t _int_id;
	uint32_t _int_vf;
	uint32_t pad;
	uint64_t pad1;
	uint32_t _int_cache[TWZ_OBJ_CACHE_SIZE];
	void *_int_sofn_cache[TWZ_OBJ_CACHE_SIZE]; // TODO: need a MUCH better way of doing this.
} twzobj;

void *twz_object_base(twzobj *);

#define TWZ_OC_HASHDATA MIP_HASHDATA
#define TWZ_OC_DFL_READ MIP_DFL_READ
#define TWZ_OC_DFL_WRITE MIP_DFL_WRITE
#define TWZ_OC_DFL_EXEC MIP_DFL_EXEC
#define TWZ_OC_DFL_USE MIP_DFL_USE
#define TWZ_OC_DFL_DEL MIP_DFL_DEL
#define TWZ_OC_ZERONONCE 0x1000
#define TWZ_OC_VOLATILE 0x2000

#define TWZ_OC_TIED_NONE 0x10000
#define TWZ_OC_TIED_VIEW 0x20000

__must_check int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id);

__must_check int twz_object_new(twzobj *obj, twzobj *src, twzobj *ku, uint64_t flags);

#define TWZ_KU_USER ((void *)0xfffffffffffffffful)

/* TODO: new API for this, audit */
_Bool objid_parse(const char *name, size_t len, objid_t *id);

/* TODO: hide */
#define twz_ptr_local(p) ({ (typeof(p))((uintptr_t)(p) & (OBJ_MAXSIZE - 1)); })
#define twz_ptr_rebase(fe, p)                                                                      \
	({ (typeof(p))((uintptr_t)SLOT_TO_VADDR(fe) | (uintptr_t)twz_ptr_local(p)); })

#define twz_ptr_islocal(p) ({ ((uintptr_t)(p) < OBJ_MAXSIZE); })

void *__twz_object_lea_foreign(twzobj *o, const void *p);

__attribute__((const)) static inline void *__twz_object_lea(twzobj *o, const void *p)
{
	if(__builtin_expect(twz_ptr_islocal(p), 1)) {
		return (void *)((uintptr_t)o->_int_base + (uintptr_t)p);
	} else {
		void *r = __twz_object_lea_foreign(o, p);
		return r;
	}
}

twzobj twz_object_from_ptr(const void *);

#define twz_object_lea(o, p) ({ (typeof(p)) __twz_object_lea((o), (p)); })

#define twz_ptr_lea(p)                                                                             \
	({                                                                                             \
		twzobj _o = twz_object_from_ptr(&(p));                                                     \
		typeof(p) _r = (typeof(p))__twz_object_lea(&_o, (p));                                      \
		_r;                                                                                        \
	});

struct metainfo *twz_object_meta(twzobj *);

int twz_object_init_guid(twzobj *obj, objid_t id, int flags);

__must_check int twz_object_init_name(twzobj *obj, const char *name, int flags);

void twz_object_release(twzobj *obj);

#define TIE_UNTIE 1

__must_check int twz_object_tie(twzobj *p, twzobj *c, int flags);
__must_check int twz_object_wire_guid(twzobj *view, objid_t id);
__must_check int twz_object_tie_guid(objid_t pid, objid_t cid, int flags);
__must_check int twz_object_wire(twzobj *, twzobj *);
__must_check int twz_object_unwire(twzobj *view, twzobj *obj);

#define TWZ_OD_IMMEDIATE 1

__must_check int twz_object_delete(twzobj *obj, int flags);
__must_check int twz_object_delete_guid(objid_t id, int flags);
objid_t twz_object_guid(twzobj *o);

int twz_object_build_alloc(twzobj *obj, size_t offset);

void twz_object_free(twzobj *obj, void *p);
__must_check __attribute__((malloc)) void *twz_object_alloc(twzobj *obj, size_t sz);

void *twz_object_getext(twzobj *obj, uint64_t tag);
__must_check int twz_object_addext(twzobj *obj, uint64_t tag, void *ptr);
int twz_object_delext(twzobj *obj, uint64_t tag, void *ptr);

enum twz_object_setsz_mode {
	TWZ_OSSM_ABSOLUTE,
	TWZ_OSSM_RELATIVE,
};

#include <twz/_types.h>
void twz_object_setsz(twzobj *obj, enum twz_object_setsz_mode mode, ssize_t amount);

// TODO: audit uses of _store_
#define TWZ_PTR_FLAGS_COPY 0xfffffffffffffffful
__must_check int __twz_ptr_store_guid(twzobj *o,
  const void **loc,
  twzobj *target,
  const void *p,
  uint64_t flags);
__must_check int __twz_ptr_store_name(twzobj *o,
  const void **loc,
  const char *name,
  const void *p,
  uint64_t flags);

__must_check int __twz_ptr_store_fote(twzobj *o,
  const void **loc,
  struct fotentry *,
  const void *p);

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

#define twz_ptr_store_fote(o, l, fe, p)                                                            \
	({                                                                                             \
		typeof(*l) _lt = p;                                                                        \
		__twz_ptr_store_fote(o, (const void **)(l), (fe), (p));                                    \
	})

#define twz_ptr_swizzle(o, p, f) ({ (typeof(p)) __twz_ptr_swizzle((o), (p), (f)); })

#include <sys/types.h>
/* TODO: make internal */
__must_check ssize_t twz_object_addfot(twzobj *obj, objid_t id, uint64_t flags);

static inline void twz_fote_init_name(struct fotentry *fe,
  char *name,
  void *resolver,
  uint64_t flags)
{
	fe->flags = flags | FE_NAME;
	fe->name.data = name;
	fe->name.nresolver = resolver;
}
