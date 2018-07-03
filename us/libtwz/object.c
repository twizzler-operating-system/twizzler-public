#include <twzobj.h>
#include <twzname.h>
#include <stdatomic.h>
#include <twzview.h>
#include <twzthread.h>
#include "blake2.h"
#include "rdrand.h"

nonce_t twz_generate_nonce(void)
{
	nonce_t nonce;
	rdrand_get_bytes(sizeof(nonce), (unsigned char *)&nonce);
	return nonce;
}

enum twz_objid_mode {
	OBJID_MODE_NONE,
	OBJID_MODE_KUONLY,
	OBJID_MODE_DATAONLY,
	OBJID_MODE_KUDATA,
};

void twz_object_metainit(struct object *obj, nonce_t nonce, objid_t kuid,
		enum twz_objid_mode im, uint64_t sz, uint64_t mdbottom, uint64_t flags)
{
	static uint64_t mif[5] = {
		[OBJID_MODE_DATAONLY] = MIF_HASHDATA | MIF_SZ,
		[OBJID_MODE_KUONLY]   = MIF_HASHKUID,
		[OBJID_MODE_NONE]     = 0,
		[OBJID_MODE_KUDATA]   = MIF_HASHDATA | MIF_HASHKUID | MIF_SZ,
	};
	struct metainfo *mi = obj->mi;
	memset(mi, 0, sizeof(*mi));

	mi->nonce = nonce;
	mi->kuid = kuid;
	mi->flags = mif[im] | flags;
	mi->sz = sz;
	mi->mdbottom = mdbottom;

	atomic_store(&mi->magic, MI_MAGIC);
}

static objid_t __twz_compute_objid_data(struct object *o, objid_t kuid, nonce_t nonce,
		enum twz_objid_mode mode, uint16_t perms)
{
	void *mptr = NULL, *data = NULL;
	if(mode == OBJID_MODE_DATAONLY || mode == OBJID_MODE_KUDATA) {
		if(o == NULL) {
			return 0;
		}
		mptr = __twz_object_meta_pointer(o, o->mi->mdbottom);
		data = __twz_ptr_lea(o, (void *)OBJ_NULLPAGE_SIZE);
	}

	unsigned char tmp[32];
	blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &nonce, sizeof(nonce));
	uint16_t flags = 0;
	switch(mode) {
		case OBJID_MODE_NONE:
			break;
		case OBJID_MODE_DATAONLY:
			flags = MIF_HASHDATA;
			blake2b_update(&S, data, o->mi->sz);
			blake2b_update(&S, mptr, o->mi->mdbottom);
			break;
		case OBJID_MODE_KUONLY:
			flags = MIF_HASHKUID;
			blake2b_update(&S, &kuid, sizeof(kuid));
			break;
		case OBJID_MODE_KUDATA:
			flags = MIF_HASHDATA | MIF_HASHKUID;
			blake2b_update(&S, &kuid, sizeof(kuid));
			blake2b_update(&S, data, o->mi->sz);
			blake2b_update(&S, mptr, o->mi->mdbottom);
			break;
	}

	uint32_t idm = MIF_FLAGS_HASHED(flags | perms);
	blake2b_update(&S, &idm, sizeof(idm));
	blake2b_final(&S, tmp, 32);
	unsigned char out[16];
	for(int i=0;i<16;i++) {
		out[i] = tmp[i] ^ tmp[i+16];
	}

	return *(objid_t *)out;
}

objid_t twz_compute_objid(struct object *o)
{
	enum twz_objid_mode mode = OBJID_MODE_NONE;
	if((o->mi->flags & MIF_HASHDATA) && (o->mi->flags & MIF_HASHKUID)) {
		mode = OBJID_MODE_KUDATA;
	} else if(o->mi->flags & MIF_HASHDATA) {
		mode = OBJID_MODE_DATAONLY;
	} else if(o->mi->flags & MIF_HASHDATA) {
		mode = OBJID_MODE_KUONLY;
	}
	if(mode == OBJID_MODE_DATAONLY || mode == OBJID_MODE_KUDATA) {
		if(!(o->mi->flags & MIF_SZ) || o->mi->mdbottom < 0x1000) {
			return 0;
		}
	}
	return __twz_compute_objid_data(o, o->mi->kuid, o->mi->nonce, mode, o->mi->flags);
}

#include <debug.h>
static int __twz_object_new(
		struct object *res,
		objid_t srcid,
		objid_t *id,
		nonce_t *nonce,
		objid_t *kuid,
		enum twz_objid_mode idmode,
		int flags)
{
	objid_t k, i;
	nonce_t n;
	int ret;
	struct object src;

	n = nonce && *nonce ? *nonce : twz_generate_nonce();
	k = kuid ? *kuid : 0;
	i = id && *id ? *id : 0;
	if(idmode == OBJID_MODE_DATAONLY || idmode == OBJID_MODE_KUDATA) {
		if(srcid == 0) {
			return -TE_INVALID;
		}
	}
	if(srcid != 0) {
		if((ret = twz_object_open(&src, srcid, 0))) {
			return ret;
		}
	}

	uint64_t omflags = srcid ? src.mi->flags : 0;
	if(flags & TWZ_ON_DFL_READ)  omflags |= MIF_DFL_READ;
	if(flags & TWZ_ON_DFL_WRITE) omflags |= MIF_DFL_WRITE;
	if(flags & TWZ_ON_DFL_EXEC)  omflags |= MIF_DFL_EXEC;
	if(flags & TWZ_ON_DFL_USE)   omflags |= MIF_DFL_USE;

	if(i == 0) {
		i = __twz_compute_objid_data(&src, k, n, idmode, omflags);
		if(id) *id = i;
	}

	if((ret = twz_obj_create(i, srcid, 0))) {
		return ret;
	}

	struct object o;
	if(!res) res = &o;

	if((ret = twz_object_open(res, i, FE_READ | FE_WRITE)) < 0) {
		return ret;
	}
	twz_object_metainit(res, n, k, idmode, srcid ? src.mi->sz : 0,
			srcid ? src.mi->mdbottom : 0, omflags);

	return 0;
}

int twz_object_new(struct object *obj, objid_t *id,
		objid_t src, objid_t kuid, int flags)
{
	enum twz_objid_mode mode = OBJID_MODE_NONE;
	if(kuid && (flags & TWZ_ON_HASHDATA)) {
		mode = OBJID_MODE_KUDATA;
	} else if(kuid) {
		mode = OBJID_MODE_KUONLY;
	} else if(flags & TWZ_ON_HASHDATA) {
		mode = OBJID_MODE_DATAONLY;
	}
	return __twz_object_new(obj, src, id, NULL, &kuid, mode, flags);
}

#include <debug.h>
void *__twz_ptr_lea(struct object *obj, void *p)
{
	/* TODO: Null pointers */
	if(twz_ptr_islocal(p)) {
		return (char *)p + (uintptr_t)obj->base - OBJ_NULLPAGE_SIZE;
	}
	ssize_t fe = twz_ptr_slot(p) - 1;
	if(fe < SLOTCACHE_SIZE && obj->slotcache[fe] > 0) {
		return (void *)((obj->slotcache[fe] * OBJ_SLOTSIZE) | (uintptr_t)__twz_ptr_local(p));
	}

	struct fotentry *fot = twz_object_fot(obj, 0);
	if(fot == NULL) {
		return NULL;
	}
	if(fe >= obj->mi->fotentries) {
		return NULL;
	}
	objid_t id = fot[fe].id;
	if(fot[fe].flags & FE_NAME) {
		/* TODO: find instances of NAME_RESOLVER_DEFAULT and replace them with actual things */
		id = twz_name_resolve(NULL, fot[fe].data, fot[fe].nresolver);
		if(id == 0) {
			return NULL;
		}
	}
	if(fot[fe].flags & FE_DERIVE) {
		//return NULL; //TODO: support
	}

	ssize_t slot = twz_view_lookupslot(id, fot[fe].flags);
	if(slot < 0) {
		return NULL;
	}
	if(fe < SLOTCACHE_SIZE) {
		obj->slotcache[fe] = slot;
	}
	return (void *)((slot * OBJ_SLOTSIZE) | (uintptr_t)__twz_ptr_local(p));
}

size_t twz_view_lookupslot(objid_t target, uint64_t flags)
{
	uint32_t tflags = ((flags & FE_READ) ? VE_READ : 0)
		| ((flags & FE_WRITE) ? VE_WRITE : 0)
		| ((flags & FE_EXEC) ? VE_EXEC : 0);
	//printf("core_lookupslot: " IDFMT " %lx\n", IDPRT(target), tflags);
	/* TODO: better algorithm */
	const int limit = 0x1fff0;
	for(int i=0x10100;i<limit;i++) {
		objid_t id;
		uint32_t fl;
		twz_view_get(NULL, i, &id, &fl);
		if((fl & VE_VALID) && id == target && (fl & (VE_READ|VE_WRITE|VE_EXEC)) == tflags) {
	//		printf("core_lookupslot: found! %ld\n", i);
			return i;
		}
		/*TODO: this is PoC benchmarkign code. remove. */
		//if(!(fl & VE_VALID) && (flags & FE_DERIVE) && i > 0x10110) {
		//	return i;
		//}
	}
	for(int i=0x10100;i<limit;i++) {
		if(twz_view_tryset(NULL, i, target, tflags) == 0) {
			return i;
		}
	}
	
	return -TE_NOSPC;
}

void *__twz_ptr_canon(struct object *o, void *p, int prot)
{
	if(twz_ptr_isnull(p)) return p;

	if(twz_ptr_isinternal(o, p)) {
		return __twz_ptr_local(p);
	} else {
		objid_t target = twz_view_virt_to_objid(NULL, p);
		if(target == 0) return NULL;
		ssize_t fe = twz_object_fot_add(o, target, prot & FE_PROTS);
		if(fe < 0) return NULL;
		return (void *)((uintptr_t)__twz_ptr_local(p) | ((fe + 1) * OBJ_SLOTSIZE));
	}
}

int twz_object_fot_add_object(struct object *obj, struct object *target, size_t *fe, int flags)
{
	objid_t targetid = twz_view_virt_to_objid(NULL, target->base);
	if(targetid == 0) {
		return -TE_INVALID;
	}
	size_t _fe = 0;
	if(fe == NULL) fe = &_fe;
	if(*fe == 0) {
		ssize_t ret = twz_object_fot_add(obj, targetid, flags);
		if(ret < 0) {
			return -TE_NOSPC;
		}
		*fe = ret+1;
	} else {
		if(twz_object_fot_set(obj, (*fe) - 1, targetid, flags) < 0)
			return -TE_INVALID;
	}
	return 0;
}

/* if container == NULL, interpret ptr as a virtual address */
int twz_ptr_swizzle(struct object *destobj, void **dest,
		struct object *container, void *ptr, int flags)
{
	struct object _d, _c;
	if(destobj == NULL) {
		destobj = &_d;
		twz_object_init(destobj, VIRT_TO_SLOT(dest));
	}
	if(container == NULL) {
		container = &_c;
		twz_object_init(container, VIRT_TO_SLOT(ptr));
	}
	ptr = twz_ptr_local(ptr);

	objid_t targetid = twz_view_virt_to_objid(NULL, container->base);
	if(targetid == 0) {
		return -TE_INVALID;
	}

	ssize_t fe = twz_object_fot_add(destobj, targetid, flags);
	if(fe < 0) {
		return -TE_NOSPC;
	}

	*dest = twz_make_canon_ptr(fe+1, (uintptr_t)ptr);
	return 0;
}

objid_t twz_view_virt_to_objid(struct object *obj, void *p)
{
	size_t slot = (uintptr_t)p / OBJ_SLOTSIZE;
	if(slot == TWZSLOT_THRD) {
		struct twzthread_repr *repr = twz_slot_to_base(TWZSLOT_THRD);
		return repr->reprid;
	}
	struct virtentry *ves = obj
		? (struct virtentry *)twz_ptr_base(obj)
		: (struct virtentry *)twz_slot_to_base(TWZSLOT_CVIEW);
	struct virtentry *v = &ves[slot];

	if(atomic_load(&v->flags) & VE_VALID) {
		return v->id;
	}
	return 0;
}

objid_t twz_object_getid(struct object *o)
{
	return twz_view_virt_to_objid(NULL, o->base);
}

int twz_view_blank(struct object *obj)
{
	objid_t vid = 0;
	int ret;

	if((ret = twz_object_new(obj, &vid, 0, 0, TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE | TWZ_ON_DFL_USE)) < 0) {
		return ret;
	}
	if((ret = twz_obj_mutate(vid, MUT_REPR_VIRT)) < 0) {
		return ret;
	}
	struct object vb;
	twz_object_open(&vb, vid, FE_READ | FE_WRITE);

	twz_view_set(&vb, TWZSLOT_CVIEW, vid, VE_READ | VE_WRITE);

	twz_view_copy(&vb, NULL, TWZSLOT_COREENTRY);
	twz_view_copy(&vb, NULL, TWZSLOT_CORETEXT);
	twz_view_copy(&vb, NULL, TWZSLOT_COREDATA);

	return 0;
}

