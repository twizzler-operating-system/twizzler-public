#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <twz/_err.h>
#include <twz/debug.h>
#include <twz/fault.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>
#include <twz/thread.h>
#include <twz/view.h>

#include <twz/persist.h>

#include <twz.h>

static void _twz_lea_fault(twzobj *o,
  const void *p,
  void *ip,
  uint32_t info,
  uint32_t retval __attribute__((unused)))
{
	size_t slot = VADDR_TO_SLOT(p);
	struct fault_pptr_info fi =
	  twz_fault_build_pptr_info(o ? twz_object_guid(o) : 0, slot, ip, info, retval, 0, NULL, p);
	twz_fault_raise(FAULT_PPTR, &fi);
}

static void obj_init(twzobj *obj, void *base, uint32_t vf, objid_t id, uint64_t flags)
{
	obj->_int_base = base;
	obj->_int_id = id;
	obj->_int_vf = vf;
	obj->_int_flags = TWZ_OBJ_VALID | flags;
	memset(obj->_int_sofn_cache, 0, sizeof(obj->_int_sofn_cache));
}

EXTERNAL void twz_object_setsz(twzobj *obj, enum twz_object_setsz_mode mode, ssize_t amount)
{
	struct metainfo *mi = twz_object_meta(obj);
	if(!(mi->flags & MIF_SZ)) {
		if(mode == TWZ_OSSM_RELATIVE) {
			struct fault_object_info fi = twz_fault_build_object_info(twz_object_guid(obj),
			  __builtin_extract_return_addr(__builtin_return_address(0)),
			  NULL,
			  FAULT_OBJECT_UNSIZED);
			twz_fault_raise(FAULT_OBJECT, &fi);
		}
		mi->flags |= MIF_SZ;
	}

	switch(mode) {
		case TWZ_OSSM_RELATIVE:
			if((ssize_t)mi->sz + amount < 0 || mi->sz + amount > OBJ_TOPDATA) {
				_twz_lea_fault(obj,
				  (void *)(mi->sz + amount),
				  __builtin_extract_return_addr(__builtin_return_address(0)),
				  FAULT_PPTR_INVALID,
				  0);
				break;
			}
			mi->sz += amount;
			break;
		case TWZ_OSSM_ABSOLUTE:
			if(amount < 0 || (size_t)amount > OBJ_TOPDATA) {
				_twz_lea_fault(obj,
				  (void *)amount,
				  __builtin_extract_return_addr(__builtin_return_address(0)),
				  FAULT_PPTR_INVALID,
				  0);
				break;
			}
			mi->sz = amount;
			break;
	}
	return;
}

EXTERNAL
void *twz_object_base(twzobj *obj)
{
	if(!(obj->_int_flags & TWZ_OBJ_VALID)) {
		_twz_lea_fault(obj,
		  NULL,
		  __builtin_extract_return_addr(__builtin_return_address(0)),
		  FAULT_PPTR_INVALID,
		  0);
	}
	return (void *)((char *)obj->_int_base + OBJ_NULLPAGE_SIZE);
}

EXTERNAL
twzobj twz_object_from_ptr(const void *p)
{
	twzobj tmp;
	obj_init(&tmp, (void *)((uintptr_t)p & ~(OBJ_MAXSIZE - 1)), 0, 0, TWZ_OBJ_NORELEASE);
	return tmp;
}

EXTERNAL
struct metainfo *twz_object_meta(twzobj *obj)
{
	if(!(obj->_int_flags & TWZ_OBJ_VALID)) {
		_twz_lea_fault(obj,
		  (void *)(OBJ_MAXSIZE - OBJ_METAPAGE_SIZE),
		  __builtin_extract_return_addr(__builtin_return_address(0)),
		  FAULT_PPTR_INVALID,
		  0);
	}
	return (struct metainfo *)((char *)obj->_int_base + OBJ_MAXSIZE - OBJ_METAPAGE_SIZE);
}

EXTERNAL
int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	/* fixup flags for system call */
	if(flags & TWZ_OC_ZERONONCE) {
		flags = (flags & ~TWZ_OC_ZERONONCE) | TWZ_SYS_OC_ZERONONCE;
	}
	if(flags & TWZ_OC_VOLATILE) {
		flags = (flags & ~TWZ_OC_VOLATILE) | TWZ_SYS_OC_VOLATILE;
	}

	int r;
	if((r = sys_ocreate(flags, kuid, src, id))) {
		return r;
	}

	/* by default, twz_object_create ties objects to the lifetime of the thread creating
	 * the object. This can be disabled, and we can tie the object to the view instead. These cover
	 * the two most common ties made for objects. */
	if(!(flags & TWZ_OC_TIED_NONE)) {
		if(flags & TWZ_OC_TIED_VIEW) {
			if((r = twz_object_wire_guid(NULL, *id))) {
				/* NOTE: if delete fails, we're pretty screwed anyway. */
				if(twz_object_delete_guid(*id, 0)) {
					libtwz_panic("failed to delete object during cleanup");
				}
				*id = 0;
				return r;
			}
		} else {
			struct twzthread_repr *repr = twz_thread_repr_base();
			if((r = twz_object_tie_guid(repr->reprid, *id, 0))) {
				if(twz_object_delete_guid(*id, 0)) {
					libtwz_panic("failed to delete object during cleanup");
				}
				*id = 0;
				return r;
			}
		}

		/* now that we've tied the object, delete it to start refcounting it */
		if(twz_object_delete_guid(*id, 0)) {
			libtwz_panic("failed to delete object whily tying");
		}
	}
	return 0;
}

EXTERNAL
int twz_object_delete_guid(objid_t id, int flags)
{
	return sys_odelete(id, flags);
}

EXTERNAL
int twz_object_delete(twzobj *obj, int flags)
{
	return twz_object_delete_guid(twz_object_guid(obj), flags);
}

EXTERNAL
int twz_object_tie_guid(objid_t pid, objid_t cid, int flags)
{
	_Static_assert(TIE_UNTIE == OTIE_UNTIE, "");
	return sys_otie(pid, cid, flags);
}

EXTERNAL
int twz_object_tie(twzobj *p, twzobj *c, int flags)
{
	_Static_assert(TIE_UNTIE == OTIE_UNTIE, "");
	return sys_otie(twz_object_guid(p), twz_object_guid(c), flags);
}

EXTERNAL
int twz_object_wire_guid(twzobj *view, objid_t id)
{
	twzobj _v;
	/* if view == NULL, we're requesting to be tied to the current view. */
	if(view == NULL) {
		view = &_v;
		twz_view_object_init(view);
	}

	return twz_object_tie_guid(twz_object_guid(view), id, 0);
}

EXTERNAL
int twz_object_wire(twzobj *view, twzobj *obj)
{
	twzobj _v;
	if(view == NULL) {
		view = &_v;
		twz_view_object_init(view);
	}
	return twz_object_tie(view, obj, 0);
}

EXTERNAL
int twz_object_unwire(twzobj *view, twzobj *obj)
{
	twzobj _v;
	if(view == NULL) {
		view = &_v;
		twz_view_object_init(view);
	}
	return twz_object_tie(view, obj, OTIE_UNTIE);
}

EXTERNAL
int twz_object_init_guid(twzobj *obj, objid_t id, int flags)
{
	ssize_t slot = twz_view_allocate_slot(NULL, id, flags);
	if(slot < 0) {
		obj->_int_flags = 0;
		return slot;
	}

	obj_init(obj, SLOT_TO_VADDR(slot), flags, id, 0);
	return 0;
}

EXTERNAL
objid_t twz_object_guid(twzobj *o)
{
	/* TODO: 128bit ATOMIC */
	if(o->_int_id)
		return o->_int_id;
	objid_t id = 0;
	if(twz_vaddr_to_obj(o->_int_base, &id, NULL)) {
		struct fault_object_info fi = twz_fault_build_object_info(0,
		  __builtin_extract_return_addr(__builtin_return_address(0)),
		  o->_int_base,
		  FAULT_OBJECT_UNKNOWN);
		twz_fault_raise(FAULT_OBJECT, &fi);
		return twz_object_guid(o);
	}
	return (o->_int_id = id);
}

EXTERNAL
int twz_object_new(twzobj *obj, twzobj *src, twzobj *ku, uint64_t flags)
{
	objid_t kuid;
	if(ku == TWZ_KU_USER) {
		const char *k = getenv("TWZUSERKU");
		if(!k) {
			return -EINVAL;
		}
		if(!objid_parse(k, strlen(k), &kuid))
			return -EINVAL;
	} else if(ku) {
		kuid = twz_object_guid(ku);
	} else {
		kuid = 0;
	}
	objid_t id;
	int r = twz_object_create(flags, kuid, src ? twz_object_guid(src) : 0, &id);
	if(r)
		return r;
	if((r = twz_object_init_guid(obj, id, FE_READ | FE_WRITE))) {
		if(twz_object_delete_guid(id, 0)) {
			libtwz_panic("failed to delete object during cleanup");
		}
		return r;
	}
	return 0;
}

EXTERNAL
int twz_object_init_name(twzobj *obj, const char *name, int flags)
{
	obj->_int_flags = 0;
	objid_t id;
	int r = twz_name_resolve(NULL, name, NULL, 0, &id);
	if(r < 0)
		return r;
	ssize_t slot = twz_view_allocate_slot(NULL, id, flags);
	if(slot < 0)
		return slot;

	obj_init(obj, SLOT_TO_VADDR(slot), flags, id, 0);
	return 0;
}

EXTERNAL
void twz_object_release(twzobj *obj)
{
	if(obj->_int_flags & TWZ_OBJ_NORELEASE) {
		libtwz_panic("tried to release an object marked no-release-needed");
	}
	if(obj->_int_flags & TWZ_OBJ_VALID) {
		twz_view_release_slot(
		  NULL, twz_object_guid(obj), obj->_int_vf, VADDR_TO_SLOT(obj->_int_base));
	}
	obj->_int_base = NULL;
	obj->_int_flags = 0;
	obj->_int_id = 0;
}

#include <twz/alloc.h>
EXTERNAL
int twz_object_build_alloc(twzobj *obj, size_t offset)
{
	/* align offset by 16 */
	offset = (offset + 15) & ~15;
	offset += OBJ_NULLPAGE_SIZE;

	struct twzoa_header *hdr = twz_object_lea(obj, (void *)offset);
	oa_hdr_init(obj, hdr, offset + sizeof(*hdr), OBJ_TOPDATA);

	return twz_object_addext(obj, ALLOC_METAINFO_TAG, (void *)offset);
}

EXTERNAL
void *twz_object_alloc(twzobj *obj, size_t sz)
{
	struct twzoa_header *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
	if(!hdr) {
		libtwz_panic("tried to allocate data in an object that does not respond to ALLOC API.");
	}

	return oa_hdr_alloc(obj, hdr, sz);
}

EXTERNAL
void twz_object_free(twzobj *obj, void *p)
{
	struct twzoa_header *hdr = twz_object_getext(obj, ALLOC_METAINFO_TAG);
	if(!hdr) {
		libtwz_panic("tried to free data in an object that does not respond to ALLOC API.");
	}

	return oa_hdr_free(obj, hdr, twz_ptr_local(p));
}

EXTERNAL
int twz_object_kaction(twzobj *obj, long cmd, ...)
{
	va_list va;
	va_start(va, cmd);
	long arg = va_arg(va, long);
	va_end(va);

	struct sys_kaction_args ka = {
		.id = twz_object_guid(obj),
		.cmd = cmd,
		.arg = arg,
		.flags = KACTION_VALID,
	};
	int r = sys_kaction(1, &ka);
	return r ? r : ka.result;
}

EXTERNAL
int twz_object_ctl(twzobj *obj, int cmd, ...)
{
	va_list va;
	va_start(va, cmd);
	long arg1 = va_arg(va, long);
	long arg2 = va_arg(va, long);
	long arg3 = va_arg(va, long);
	va_end(va);

	return sys_octl(twz_object_guid(obj), cmd, arg1, arg2, arg3);
}

EXTERNAL
int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags)
{
	uintptr_t pa;
	int r = sys_opin(twz_object_guid(obj), &pa, flags);
	if(oaddr) {
		*oaddr = pa + OBJ_NULLPAGE_SIZE;
	}
	return r;
}

EXTERNAL
void *twz_object_getext(twzobj *obj, uint64_t tag)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct metaext *e = &mi->exts[0];

	while((char *)e < (char *)mi + mi->milen) {
		void *p = atomic_load(&e->ptr);
		if(atomic_load(&e->tag) == tag && p) {
			return twz_object_lea(obj, p);
		}
		e++;
	}
	return NULL;
}

EXTERNAL
int twz_object_addext(twzobj *obj, uint64_t tag, void *ptr)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct metaext *e = &mi->exts[0];

	while((char *)e < (char *)mi + mi->milen) {
		if(atomic_load(&e->tag) == 0) {
			uint64_t exp = 0;
			if(atomic_compare_exchange_strong(&e->tag, &exp, tag)) {
				atomic_store(&e->ptr, twz_ptr_local(ptr));
				_clwb(e);
				_pfence();
				return 0;
			}
		}
		e++;
	}
	return -ENOSPC;
}

EXTERNAL
int twz_object_delext(twzobj *obj, uint64_t tag, void *ptr)
{
	struct metainfo *mi = twz_object_meta(obj);
	struct metaext *e = &mi->exts[0];

	while((char *)e < (char *)mi + mi->milen) {
		if(atomic_load(&e->tag) == tag) {
			void *exp = ptr;
			if(atomic_compare_exchange_strong(&e->ptr, &exp, NULL)) {
				atomic_store(&e->tag, 0);
				_clwb(e);
				_pfence();
				return 0;
			}
		}
		e++;
	}
	return -ENOENT;
}

/* FOT rules:
 * * FOT additions and scans are mutually synchronized.
 * * FOT updates and deletions require synchronization external to this library.
 * Rationale: updates and deletions require either application-specific logic anyway or
 * are done offline.
 */

static ssize_t _twz_object_scan_fot(twzobj *obj, objid_t id, uint64_t flags)
{
	struct metainfo *mi = twz_object_meta(obj);
	for(size_t i = 1; i < mi->fotentries; i++) {
		struct fotentry *fe = _twz_object_get_fote(obj, i);
		if((atomic_load(&fe->flags) & _FE_VALID) && !(fe->flags & FE_NAME) && fe->id == id
		   && (fe->flags & ~(_FE_VALID | _FE_ALLOC)) == flags) {
			return i;
		}
	}
	return -1;
}

EXTERNAL
ssize_t twz_object_addfot(twzobj *obj, objid_t id, uint64_t flags)
{
	ssize_t r = _twz_object_scan_fot(obj, id, flags);
	if(r > 0) {
		return r;
	}
	struct metainfo *mi = twz_object_meta(obj);

	flags &= ~_FE_VALID;
	while(1) {
		uint32_t i = atomic_fetch_add(&mi->fotentries, 1);
		/* TODO: is this safe? */
		if(i == 0)
			i = atomic_fetch_add(&mi->fotentries, 1);
		if(i == OBJ_MAXFOTE)
			return -ENOSPC;
		struct fotentry *fe = _twz_object_get_fote(obj, i);
		if(!(atomic_fetch_or(&fe->flags, _FE_ALLOC) & _FE_ALLOC)) {
			/* successfully allocated */
			fe->id = id;
			fe->flags = flags;
			fe->info = 0;
			/* flush the new entry */
			_clwb(fe);
			_pfence();
			atomic_fetch_or(&fe->flags, _FE_VALID);
			_clwb(fe);
			_pfence();
			/* flush the valid bit */
			return i;
		}
	}
}

static int __twz_ptr_make(twzobj *obj, objid_t id, const void *p, uint32_t flags, const void **res)
{
	ssize_t fe = twz_object_addfot(obj, id, flags);
	if(fe < 0)
		return fe;

	*res = twz_ptr_rebase(fe, p);
	_clwb(res);
	_pfence();

	return 0;
}

EXTERNAL
int __twz_ptr_store_guid(twzobj *obj, const void **res, twzobj *tgt, const void *p, uint64_t flags)
{
	objid_t target;
	if(!tgt) {
		int r = twz_vaddr_to_obj(p, &target, NULL);
		if(r) {
			return r;
		}
	}

	return __twz_ptr_make(obj, tgt ? twz_object_guid(tgt) : target, p, flags, res);
}

EXTERNAL
int __twz_ptr_store_fote(twzobj *obj, const void **res, struct fotentry *f, const void *p)
{
	ssize_t fe = twz_object_addfot(obj, 0, 0);
	if(fe < 0)
		return fe;

	struct fotentry *pf = _twz_object_get_fote(obj, fe);
	f->flags |= _FE_VALID | _FE_ALLOC;
	*pf = *f;
	_clwb_len(pf, sizeof(*pf));
	_pfence();
	*res = twz_ptr_rebase(fe, p);
	_clwb(res);
	_pfence();

	return 0;
}

/* TODO: if target is the same as obj, just store a local pointer */
EXTERNAL
void *__twz_ptr_swizzle(twzobj *obj, const void *p, uint64_t flags)
{
	objid_t target;
	int r = twz_vaddr_to_obj(p, &target, NULL);
	if(r) {
		_twz_lea_fault(NULL,
		  p,
		  __builtin_extract_return_addr(__builtin_return_address(0)),
		  FAULT_PPTR_INVALID,
		  r);
		return NULL;
	}

	ssize_t fe = twz_object_addfot(obj, target, flags);
	if(fe < 0) {
		_twz_lea_fault(obj,
		  NULL,
		  __builtin_extract_return_addr(__builtin_return_address(0)),
		  FAULT_PPTR_RESOURCES,
		  r);
		return NULL;
	}
	return twz_ptr_rebase(fe, (void *)p);
}

EXTERNAL
void *__twz_object_lea_foreign(twzobj *o, const void *p)
{
	size_t slot = VADDR_TO_SLOT(p);
#if 1
	if(o->_int_flags & TWZ_OBJ_CACHE) {
		if(slot < TWZ_OBJ_CACHE_SIZE && o->_int_cache[slot]) {
			return twz_ptr_rebase(o->_int_cache[slot], (void *)p);
		}
	} else {
		memset(o->_int_cache, 0, sizeof(o->_int_cache));
		o->_int_flags |= TWZ_OBJ_CACHE;
	}

#endif

	struct metainfo *mi = twz_object_meta(o);
	struct fotentry *fe = _twz_object_get_fote(o, slot);

	uint64_t info = FAULT_PPTR_INVALID;

	int r = 0;
	if(__builtin_expect(slot >= mi->fotentries, 0)) {
		goto fault;
	}

	if(__builtin_expect(!(atomic_load(&fe->flags) & _FE_VALID) || fe->id == 0, 0)) {
		goto fault;
	}

	objid_t id;
	if(__builtin_expect(fe->flags & FE_NAME, 0)) {
		void *vptr;
		if((r = twz_fot_indirect_resolve(o, fe, p, &vptr, &info))) {
			goto fault;
		}
		return vptr;
	}

	id = fe->id;
	if(__builtin_expect(fe->flags & FE_DERIVE, 0)) {
		/* Currently, the derive bit can only be used for executables in slot 0. This may change
		 * in the future. */
		info = FAULT_PPTR_DERIVE;
		goto fault;
	}

	ssize_t ns = twz_view_allocate_slot(NULL, id, fe->flags & (FE_READ | FE_WRITE | FE_EXEC));
	if(ns < 0) {
		info = FAULT_PPTR_RESOURCES;
		goto fault;
	}

	void *_r = twz_ptr_rebase(ns, (void *)p);
	if(slot < TWZ_OBJ_CACHE_SIZE) {
		o->_int_cache[slot] = ns;
	}
	return _r;
fault:
	_twz_lea_fault(o, p, __builtin_extract_return_addr(__builtin_return_address(0)), info, r);
	return NULL;
}
