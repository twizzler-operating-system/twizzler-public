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

int twz_object_create(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	if(flags & TWZ_OC_ZERONONCE) {
		flags = (flags & ~TWZ_OC_ZERONONCE) | TWZ_SYS_OC_ZERONONCE;
	}
	if(flags & TWZ_OC_VOLATILE) {
		flags = (flags & ~TWZ_OC_VOLATILE) | TWZ_SYS_OC_VOLATILE;
	}
	return sys_ocreate(flags, kuid, src, id);
}

int twz_object_init_guid(twzobj *obj, objid_t id, int flags)
{
	ssize_t slot = twz_view_allocate_slot(NULL, id, flags);
	if(slot < 0) {
		obj->flags = 0;
		return slot;
	}

	obj->base = (void *)(OBJ_MAXSIZE * (slot));
	obj->id = id;
	obj->flags = TWZ_OBJ_VALID;
	obj->vf = flags;
	return 0;
}

objid_t twz_object_guid(twzobj *o)
{
	/* TODO: 128bit ATOMIC */
	if(o->id)
		return o->id;
	objid_t id = 0;
	if(twz_vaddr_to_obj(o->base, &id, NULL)) {
		struct fault_object_info fi = {
			.objid = 0,
			.ip = (uintptr_t)&twz_object_guid,
			.addr = (uintptr_t)o->base,
			.flags = FAULT_OBJECT_UNKNOWN,
		};
		twz_fault_raise(FAULT_OBJECT, &fi);
		return twz_object_guid(o);
	}
	return (o->id = id);
}

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
	}
	objid_t id;
	int r = twz_object_create(flags, kuid, src ? twz_object_guid(src) : 0, &id);
	if(r)
		return r;
	return twz_object_init_guid(obj, id, FE_READ | FE_WRITE);
}

int twz_object_init_name(twzobj *obj, const char *name, int flags)
{
	objid_t id;
	int r = twz_name_resolve(NULL, name, NULL, 0, &id);
	if(r < 0)
		return r;
	ssize_t slot = twz_view_allocate_slot(NULL, id, flags);
	obj->flags = 0;
	if(slot < 0)
		return slot;

	obj->vf = flags;
	obj->base = (void *)(OBJ_MAXSIZE * (slot));
	obj->id = id;
	obj->flags = TWZ_OBJ_VALID;
	return 0;
}

void twz_object_release(twzobj *obj)
{
	twz_view_release_slot(NULL, twz_object_guid(obj), obj->vf, twz_base_to_slot(obj->base));
	obj->base = NULL;
	obj->flags = 0;
}

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

int twz_object_pin(twzobj *obj, uintptr_t *oaddr, int flags)
{
	uintptr_t pa;
	int r = sys_opin(twz_object_guid(obj), &pa, flags);
	if(oaddr)
		*oaddr = pa + OBJ_NULLPAGE_SIZE;
	return r;
}

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

int __twz_ptr_store_guid(twzobj *obj, const void **res, twzobj *tgt, const void *p, uint64_t flags)
{
	objid_t target;
	if(!tgt) {
		int r = twz_vaddr_to_obj(p, &target, NULL);
		if(r)
			return r;
	}

	return __twz_ptr_make(obj, tgt ? twz_object_guid(tgt) : target, p, flags, res);
}

void *__twz_ptr_swizzle(twzobj *obj, const void *p, uint64_t flags)
{
	objid_t target;
	int r = twz_vaddr_to_obj(p, &target, NULL);
	if(r)
		return r;

	ssize_t fe = twz_object_addfot(obj, target, flags);
	if(fe < 0)
		return fe;
	return twz_ptr_rebase(fe, p);
}

static void _twz_lea_fault(twzobj *o, const void *p, uintptr_t ip, uint32_t info, uint32_t retval)
{
	size_t slot = (uintptr_t)p / OBJ_MAXSIZE;
	struct fault_pptr_info fi = {
		.objid = twz_object_guid(o), .fote = slot, .ptr = p, .info = info, .ip = ip
	};
	twz_fault_raise(FAULT_PPTR, &fi);
}

void *__twz_object_lea_foreign(twzobj *o, const void *p)
{
#if 1
	if(o->flags & TWZ_OBJ_CACHE) {
		size_t fe = twz_base_to_slot(p);
		if(fe < TWZ_OBJ_CACHE_SIZE && o->cache[fe]) {
			return twz_ptr_rebase(o->cache[fe], p);
		}
	} else {
		memset(o->cache, 0, sizeof(o->cache));
		o->flags |= TWZ_OBJ_CACHE;
	}

#endif

	struct metainfo *mi = twz_object_meta(o);
	size_t slot = (uintptr_t)p / OBJ_MAXSIZE;
	struct fotentry *fe = _twz_object_get_fote(o, slot);

	uint64_t info = FAULT_PPTR_INVALID;
	const char *name;

	int r = 0;
	if(__builtin_expect(slot >= mi->fotentries, 0)) {
		goto fault;
	}

	if(__builtin_expect(!(atomic_load(&fe->flags) & _FE_VALID) || fe->id == 0, 0)) {
		goto fault;
	}

	objid_t id;
	if(__builtin_expect(fe->flags & FE_NAME, 0)) {
		r = twz_name_resolve(o, fe->name.data, fe->name.nresolver, 0, &id);
		if(r) {
			info = FAULT_PPTR_RESOLVE;
			name = fe->name.data;
			goto fault;
		}
	} else {
		id = fe->id;
	}

	if(__builtin_expect(fe->flags & FE_DERIVE, 0)) {
		/* Currently, the derive bit can only be used for executables in slot 0. This may change in
		 * the future. */
		info = FAULT_PPTR_DERIVE;
		goto fault;
	}

	ssize_t ns = twz_view_allocate_slot(NULL, id, fe->flags & (FE_READ | FE_WRITE | FE_EXEC));
	if(ns < 0) {
		info = FAULT_PPTR_RESOURCES;
	}

	size_t e = twz_base_to_slot(p);
	void *_r = twz_ptr_rebase(ns, (void *)p);
	if(e < TWZ_OBJ_CACHE_SIZE) {
		o->cache[e] = ns;
	}
	return _r;
fault:
	_twz_lea_fault(o, p, (uintptr_t)__builtin_return_address(0), info, r);
	return NULL;
}
