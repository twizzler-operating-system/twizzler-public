#pragma once

#ifndef _TWZ_MUSL_INTERNAL
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif
#include <string.h>
#include <twz.h>
#include <twzerr.h>
#include <twzsys.h>

#define OBJ_SLOTSIZE (1024ul * 1024 * 1024)
#define OBJ_METAHEADER_END 1024
#define OBJ_NULLPAGE_SIZE 0x1000
#define OBJ_METAPAGE_SIZE 0x1000

#define MI_MAGIC 0x54575A4F
#define MIF_FOT    1
#define MIF_SZ     2
#define MIF_HTABLE 4
#define MIF_HASHDATA  8
#define MIF_HASHKUID 16
#define MIF_DFL_READ  0x20
#define MIF_DFL_WRITE 0x40
#define MIF_DFL_EXEC  0x80
#define MIF_DFL_USE   0x100

static inline uint32_t MIF_FLAGS_HASHED(uint16_t flags)
{
	uint32_t ret;
	if(flags & MIF_HASHDATA) {
		if(flags & MIF_HASHKUID) {
			ret = 3;
		} else {
			ret = 1;
		}
	} else {
		if(flags & MIF_HASHKUID) {
			ret = 2;
		} else {
			ret = 0;
		}
	}
	ret |= (flags & (MIF_DFL_READ | MIF_DFL_WRITE | MIF_DFL_USE | MIF_DFL_EXEC));
	return ret;
}

typedef unsigned __int128 nonce_t;

struct metainfo {
	uint32_t magic;
	uint16_t flags;
	uint16_t milen;
	uint32_t mdbottom;
	uint32_t fotentries;
	uint64_t sz;
	uint32_t nbuckets;
	uint32_t hashstart;
	nonce_t nonce;
	objid_t kuid;
	char data[];
} __attribute__((packed));

#define SLOTCACHE_SIZE 16
struct object {
	struct metainfo *mi;
	void *base;
	size_t slotcache[SLOTCACHE_SIZE];
};

struct metaheader {
	uint16_t len;
	uint16_t id;
	uint32_t pad0;
	uint64_t pad1;
} __attribute__((packed));

struct metavar_bucket {
	uint32_t offset;
	uint16_t namelen;
	uint16_t datalen;
} __attribute__((packed));


nonce_t twz_generate_nonce(void);
ssize_t twz_view_lookupslot(objid_t target, uint64_t flags);

static inline void *__twz_object_meta_pointer(struct object *o, size_t off)
{
	return (void *)((char *)o->base + OBJ_SLOTSIZE - (off + OBJ_NULLPAGE_SIZE));
}

static inline unsigned long __twz_metavar_hash(const char *data, size_t len)
{
	unsigned long hash = 5381;
	while(len-- > 0) {
		hash = ((hash << 5) + hash) ^ *data++;
	}
	return hash;
}

static inline const char *twz_object_lea_metavar(struct object *o, const char *key,
		size_t keysz)
{
	if(!(o->mi->flags & MIF_HTABLE)) {
		return NULL;
	}
	unsigned long hash = __twz_metavar_hash(key, keysz);
	struct metavar_bucket *table = __twz_object_meta_pointer(o, o->mi->hashstart);
	unsigned b = hash % o->mi->nbuckets;
	do {
		struct metavar_bucket *entry = &table[b];
		if(entry->offset == 0) return NULL;
		if(entry->offset != ~0u) {
			const char *ek = __twz_object_meta_pointer(o, entry->offset);
			if(entry->namelen == keysz
					&& !memcmp(ek, key, keysz)) {
				return ek;
			}
		}
		b = (b+1) % o->mi->nbuckets;
	} while(b != hash % o->mi->nbuckets);
	return NULL;
}

#define twz_obj_get_metavar(o, name, namelen, ptr) \
	({ const char *v = twz_object_lea_metavar(o, name, namelen); \
	   if(v) \
	   memcpy(ptr, (v)+namelen, sizeof(*(ptr))); !!v;\
	})

#define twz_obj_get_metavar_strname(o, name, ptr) \
	({ const char *v = twz_object_lea_metavar(o, name, strlen(name)); \
	   if(v) \
	   memcpy(ptr, (v)+strlen(name), sizeof(*(ptr))); !!v;\
	})

#define FE_NAME     1
#define FE_DERIVE   2
#define FE_READ     4
#define FE_WRITE    8
#define FE_EXEC  0x10

#define FE_PROTS (FE_READ|FE_WRITE|FE_EXEC)

struct fotentry {
	union {
		objid_t id;
		char data[16];
	};

	uint64_t nresolver;
	uint64_t flags;
};

static inline void *twz_slot_to_base(size_t slot)
{
	return (void *)(slot * OBJ_SLOTSIZE + OBJ_NULLPAGE_SIZE);
}

static inline struct metainfo *twz_slot_to_meta(size_t slot)
{
	return (struct metainfo *)((slot + 1) * OBJ_SLOTSIZE - OBJ_METAPAGE_SIZE);
}

	/* TODO: thread safe */
static inline struct fotentry *twz_object_fot(struct object *o, bool make)
{
	if(make && !(o->mi->flags & MIF_FOT)) {
		o->mi->flags |= MIF_FOT;
		o->mi->fotentries = 0;
	}
	return (o->mi->flags & MIF_FOT) ? (struct fotentry *)((char *)o->mi + OBJ_METAHEADER_END) : NULL;
}

/* TODO: thread safe */
static inline ssize_t twz_object_fot_add(struct object *o, objid_t id, int fl)
{
	/* TODO: this should search for a matching entry first */
	struct fotentry *fot = twz_object_fot(o, 1);
	unsigned fe = o->mi->fotentries++;
	if(fe > 90) return -TE_NOSPC; //TODO: support large FOTs
	fot[fe].nresolver = 0;
	fot[fe].flags = fl;
	fot[fe].id = id;
	return fe;
}

/* TODO: thread safe */
static inline int twz_object_fot_set(struct object *o, size_t fe, objid_t id, int fl)
{
	/* TODO: this should search for a matching entry first */
	struct fotentry *fot = twz_object_fot(o, 1);
	if(fe >= o->mi->fotentries) {
		o->mi->fotentries = fe+1;
	}
	if(fe > 90) return -TE_NOSPC; //TODO: support large FOTs
	fot[fe].nresolver = 0;
	fot[fe].flags = fl;
	fot[fe].id = id;
	return 0;
}

/* TODO: thread safe */
static inline ssize_t twz_object_fot_add_name(struct object *o, const char *name,
		uint64_t nresolver, int fl)
{
	/* TODO: this should search for a matching entry first */
	struct fotentry *fot = twz_object_fot(o, 1);
	unsigned fe = o->mi->fotentries++;
	if(fe > 90) return -TE_NOSPC; //TODO: support large FOTs
	fot[fe].nresolver = nresolver;
	fot[fe].flags = fl | FE_NAME;
	strncpy(fot[fe].data, name, 16);
	return fe;
}

static inline void *twz_object_addmeta(struct object *o, struct metaheader hdr)
{
	struct metaheader *mh;
	for(uint16_t start = 0;
			start < OBJ_METAHEADER_END - sizeof(struct metaheader);
			start += mh->len) {
		mh = (struct metaheader *)(o->mi->data + start);
		if(mh->id == 0 || mh->len == 0) {
			mh->id = hdr.id;
			/* align on 128 bits */
			mh->len = ((hdr.len - 1) & ~15) + 16;
			return mh;
		}
	}
	return NULL;
}

static inline void *twz_object_findmeta(struct object *o, uint16_t id)
{
	struct metaheader *mh;
	for(uint16_t start = 0;
			start < OBJ_METAHEADER_END - sizeof(struct metaheader);
			start += mh->len) {
		mh = (struct metaheader *)(o->mi->data + start);
		if(mh->id == 0 || mh->len == 0) {
			return NULL;
		}
		if(mh->id == id) return mh;
	}
	return NULL;
}

static inline bool twz_ptr_isinternal(struct object *o, void *p)
{
	return ((char *)p >= (char *)o->base
			&& (char *)p < (char *)o->base + (OBJ_SLOTSIZE - OBJ_NULLPAGE_SIZE));
}

static inline bool twz_ptr_islocal(void *p)
{
	return (uintptr_t)p < OBJ_SLOTSIZE;
}

static inline size_t twz_ptr_slot(void *p)
{
	return (uintptr_t)p / OBJ_SLOTSIZE;
}

static inline void twz_object_init(struct object *o, size_t slot)
{
	o->mi = twz_slot_to_meta(slot);
	o->base = twz_slot_to_base(slot);
	memset(o->slotcache, 0, sizeof(o->slotcache));
}

#define TWZ_OBJECT_INIT(slot) \
	(struct object) { \
		.mi = (void *)((slot+1) * OBJ_SLOTSIZE - OBJ_METAPAGE_SIZE), \
		.base = (void *)(slot * OBJ_SLOTSIZE + OBJ_NULLPAGE_SIZE) \
	}

static inline void *__twz_ptr_local(void *p)
{
	return (void *)((uintptr_t)p & (OBJ_SLOTSIZE - 1));
}

/* TODO: arch-dep? */
#define KSO_VIEW 1
#define KSO_SCTX 2
#define KSO_THRD 3

#define KSO_TYPE(x) ({ (x) << 8; })

static inline int twz_obj_create(objid_t id, objid_t src, int flags)
{
	return sys_ocreate(id, src, flags);
}

static inline int twz_object_open(struct object *obj, objid_t id, uint64_t flags)
{
	ssize_t slot = twz_view_lookupslot(id, flags);
	if(slot < 0) {
		return slot;
	}
	twz_object_init(obj, slot);
	return 0;
}

void *__twz_ptr_lea(struct object *obj, void *p);
void *__twz_ptr_canon(struct object *o, void *p, int prot);

#define twz_ptr_base(o) ((o)->base)

#define twz_ptr_isnull(p) ({ ((uintptr_t)(p) & (OBJ_SLOTSIZE - 1)) < OBJ_NULLPAGE_SIZE; })

#define twz_make_canon_ptr(fe,off) \
	({ (void *) ((fe) * OBJ_SLOTSIZE | (off)); })

#define twz_ptr_local(p) \
	({ (typeof(p))__twz_ptr_local(p); })

#define twz_ptr_canon(o,p,prot) \
	({ (typeof(p))__twz_ptr_canon(o, p, prot); })

#define twz_ptr_lea(o, p) \
	({ (typeof(p))__twz_ptr_lea(o, p); })

#define twz_ptr_load(o,p) \
	({ *(typeof(p))__twz_ptr_lea(o, p); })

#define twz_ptr_store(o,p,v) \
	({ *(typeof(p))__twz_ptr_lea(o, p) = v; })

#define twz_ptr_rebase(s,p) \
	({ (typeof(p))twz_make_canon_ptr(s, (uintptr_t)__twz_ptr_local(p)); })

#define TWZ_ON_HASHDATA 1
#define TWZ_ON_DFL_READ 2
#define TWZ_ON_DFL_WRITE 4
#define TWZ_ON_DFL_EXEC 8
#define TWZ_ON_DFL_USE  0x10





int twz_object_new(struct object *obj, objid_t *id, objid_t src, objid_t kuid, int flags);

objid_t twz_compute_objid(struct object *o);


static inline bool twz_object_same(struct object *a, struct object *b)
{
	return a->base == b->base;
}


int twz_ptr_swizzle(struct object *destobj, void **dest,
		struct object *container, void *ptr, int flags);

int twz_object_fot_add_object(struct object *obj, struct object *target, size_t *fe, int flags);

objid_t twz_object_getid(struct object *o);


bool twz_objid_parse(const char *name, objid_t *id);
