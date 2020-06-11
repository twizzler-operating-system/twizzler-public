#include <kalloc.h>
#include <lib/bitmap.h>
#include <lib/blake2.h>
#include <lib/iter.h>
#include <lib/rb.h>
#include <memory.h>
#include <object.h>
#include <page.h>
#include <pager.h>
#include <processor.h>
#include <slab.h>
#include <slots.h>
#include <tmpmap.h>
#include <twz/_sys.h>

static _Atomic size_t obj_count = 0;
static struct rbroot obj_tree = RBINIT;

static struct slabcache sc_objs;

static struct spinlock objlock = SPINLOCK_INIT;

void obj_print_stats(void)
{
	printk("KNOWN OBJECTS: %ld\n", obj_count);
}

static int __obj_compar_key(struct object *a, objid_t b)
{
	if(a->id > b)
		return 1;
	else if(a->id < b)
		return -1;
	return 0;
}

static int __obj_compar(struct object *a, struct object *b)
{
	return __obj_compar_key(a, b->id);
}

static void _obj_ctor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	obj->lock = SPINLOCK_INIT;
	obj->tslock = SPINLOCK_INIT;
	obj->pagecache_root = RBINIT;
	obj->pagecache_level1_root = RBINIT;
	obj->tstable_root = RBINIT;
	obj->page_requests_root = RBINIT;
}

void obj_init(struct object *obj)
{
	_obj_ctor(NULL, obj);
	obj->slot = NULL;
	obj->flags = 0;
	obj->id = 0;
	obj->kvmap = NULL;
	obj->kaddr = NULL;
	obj->sourced_from = NULL;
	krc_init(&obj->refs);
	krc_init_zero(&obj->mapcount);
	arch_object_init(obj);
	obj->ties_root = RBINIT;
	obj->idx_map = RBINIT;
	list_init(&obj->derivations);
}

static void _obj_dtor(void *_u, void *ptr)
{
	(void)_u;
	struct object *obj = ptr;
	assert(krc_iszero(&obj->refs));
	assert(krc_iszero(&obj->mapcount));
}

void obj_system_init(void)
{
	slabcache_init(&sc_objs, "sc_objs", sizeof(struct object), _obj_ctor, _obj_dtor, NULL);
	obj_system_init_objpage();
}

static struct kso_calls *_kso_calls[KSO_MAX];

void kso_register(int t, struct kso_calls *c)
{
	_kso_calls[t] = c;
}

struct kso_calls *kso_lookup_calls(int t)
{
	return _kso_calls[t];
}

void kso_detach_event(struct thread *thr, bool entry, int sysc)
{
	for(size_t i = 0; i < KSO_MAX; i++) {
		if(_kso_calls[i] && _kso_calls[i]->detach_event) {
			_kso_calls[i]->detach_event(thr, entry, sysc);
		}
	}
}

void obj_kso_init(struct object *obj, enum kso_type ksot)
{
	obj->kso_type = ksot;
	obj->kso_calls = _kso_calls[ksot];
	if(obj->kso_calls && obj->kso_calls->ctor) {
		obj->kso_calls->ctor(obj);
	}
}

static inline struct object *__obj_alloc(enum kso_type ksot, objid_t id)
{
	struct object *obj = slabcache_alloc(&sc_objs);

	obj_init(obj);
	obj->id = id;
	obj_kso_init(obj, ksot);

	obj_count++;

	return obj;
}

struct object *obj_create(uint128_t id, enum kso_type ksot)
{
	struct object *obj = __obj_alloc(ksot, id);
	/* TODO (major): check for duplicates */
	if(id) {
		spinlock_acquire_save(&objlock);
		rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
		spinlock_release_restore(&objlock);
	}
	return obj;
}

void obj_assign_id(struct object *obj, objid_t id)
{
	spinlock_acquire_save(&objlock);
	if(obj->id) {
		panic("tried to reassign object ID");
	}
	obj->id = id;
	rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
	spinlock_release_restore(&objlock);
}

struct object *obj_create_clone(uint128_t id, objid_t srcid, enum kso_type ksot)
{
	struct object *src = obj_lookup(srcid, 0);
	if(src == NULL) {
		return NULL;
	}
	struct object *obj = __obj_alloc(ksot, id);

	obj_clone_cow(src, obj);

	// if(src->flags & OF_PAGER) {
	krc_get(&src->refs);
	obj->sourced_from = src;
	//} else {
	//	obj_put(src);
	//}

	if(id) {
		spinlock_acquire_save(&objlock);
		rb_insert(&obj_tree, obj, struct object, node, __obj_compar);
		spinlock_release_restore(&objlock);
	}
	return obj;
}

struct object *obj_lookup(uint128_t id, int flags)
{
	spinlock_acquire_save(&objlock);
	struct rbnode *node = rb_search(&obj_tree, id, struct object, node, __obj_compar_key);

	struct object *obj = node ? rb_entry(node, struct object, node) : NULL;
	if(node) {
		krc_get(&obj->refs);
		spinlock_release_restore(&objlock);

		spinlock_acquire_save(&obj->lock);
		if((obj->flags & OF_HIDDEN) && !(flags & OBJ_LOOKUP_HIDDEN)) {
			spinlock_release_restore(&obj->lock);
			obj_put(obj);
			return NULL;
		}
		spinlock_release_restore(&obj->lock);
	} else {
		spinlock_release_restore(&objlock);
	}
	return obj;
}

void obj_release_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);

	assert(obj->slot);
	assert(obj->mapcount.count > 0);

	// printk("REL_SLOT " IDFMT "; mapcount=%ld\n", IDPR(obj->id), obj->mapcount.count);
	if(krc_put(&obj->mapcount)) {
		/* hit zero; release */
		struct slot *slot = obj->slot;
		obj->slot = NULL;
		/* this krc drop MUST not reduce the count to 0, because there must be a reference to obj
		 * coming into this function */
		object_space_release_slot(slot);
		slot_release(slot);
		bool tmp = krc_put(&obj->refs);
		if(tmp) {
			panic("tmp nonzero for object " IDFMT "\n", IDPR(obj->id));
		}
		assert(!tmp);
#if CONFIG_DEBUG_OBJECT_SLOT
		printk("MAPCOUNT ZERO: " IDFMT "; refs=%ld\n", IDPR(obj->id), obj->refs.count);
#endif
	}

	spinlock_release_restore(&obj->lock);
}

struct slot *obj_alloc_slot(struct object *obj)
{
	spinlock_acquire_save(&obj->lock);
	if(!obj->slot) {
		assert(obj->mapcount.count == 0);
		obj->slot = slot_alloc();
		krc_get(&obj->refs);
		obj->slot->obj = obj; /* above get */
	}

	krc_get(&obj->mapcount);

#if CONFIG_DEBUG_OBJECT_SLOT
	printk("[slot]: allocated uslot %ld for " IDFMT " (%p %lx %d), mapcount %ld\n",
	  obj->slot->num,
	  IDPR(obj->id),
	  obj,
	  obj->flags,
	  obj->kso_type,
	  obj->mapcount.count);
#endif
	krc_get(&obj->slot->rc); /* return */
	spinlock_release_restore(&obj->lock);
	return obj->slot;
}

static void _obj_release(void *_obj)
{
	struct object *obj = _obj;
#if CONFIG_DEBUG_OBJECT_LIFE
	printk("OBJ RELEASE: " IDFMT "\n", IDPR(obj->id));
#endif
	if(obj->flags & OF_DELETE) {
#if CONFIG_DEBUG_OBJECT_LIFE
		printk("FINAL DELETE object " IDFMT "\n", IDPR(obj->id));
#endif

		obj_tie_free(obj);

		arch_object_unmap_all(obj);
		//		printk("releasin object pages\n");
		struct rbnode *next;
		for(struct rbnode *node = rb_first(&obj->pagecache_root); node; node = next) {
			next = rb_next(node);
			struct objpage *pg = rb_entry(node, struct objpage, node);
			//		printk(":: %ld\n", pg->refs.count);
			rb_delete(node, &obj->pagecache_root);
			objpage_release(pg, 0);
		}

		for(struct rbnode *node = rb_first(&obj->pagecache_level1_root); node; node = next) {
			next = rb_next(node);
			struct objpage *pg = rb_entry(node, struct objpage, node);
			//		printk(":: %ld\n", pg->refs.count);
			rb_delete(node, &obj->pagecache_root);
			objpage_release(pg, 0);
		}

		struct list *lnext;
		if(obj->sourced_from) {
			spinlock_acquire_save(&obj->sourced_from->lock);
			for(struct list *e = list_iter_start(&obj->sourced_from->derivations);
			    e != list_iter_end(&obj->sourced_from->derivations);
			    e = lnext) {
				lnext = list_iter_next(e);
				struct derivation_info *di = list_entry(e, struct derivation_info, entry);
				if(di->id == obj->id) {
					list_remove(&di->entry);
					kfree(di);
				}
			}

			spinlock_release_restore(&obj->sourced_from->lock);
			obj_put(obj->sourced_from);
			obj->sourced_from = NULL;
		}
#if 0
		printk("obj release " IDFMT ":: %p %p\n",
		  IDPR(obj->id),
		  list_iter_start(&obj->derivations),
		  list_iter_end(&obj->derivations));
#endif
		for(struct list *e = list_iter_start(&obj->derivations);
		    e != list_iter_end(&obj->derivations);
		    e = lnext) {
			lnext = list_iter_next(e);
			struct derivation_info *di = list_entry(e, struct derivation_info, entry);
			//	printk("freed derivation %p\n", di);
			list_remove(&di->entry);
			kfree(di);
		}

		arch_object_destroy(obj);
		obj_count--;
		/* TODO: clean up... */
		slabcache_free(&sc_objs, obj);
	}
}

void obj_put(struct object *o)
{
	if(krc_put_locked(&o->refs, &objlock)) {
		if(o->flags & OF_DELETE) {
			rb_delete(&o->node, &obj_tree);
		}
		spinlock_release_restore(&objlock);
		// workqueue_insert(&current_processor->wq, &o->delete_task, _obj_release, o);
		_obj_release(o);
	}
}

bool obj_get_pflags(struct object *obj, uint32_t *pf)
{
	*pf = 0;
	spinlock_acquire_save(&obj->lock);
	if(obj->flags & OF_CPF_VALID) {
		*pf = obj->cached_pflags;
		spinlock_release_restore(&obj->lock);
		return true;
	}

	spinlock_release_restore(&obj->lock);
	struct metainfo mi;
	obj_read_data(obj, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	if(mi.magic != MI_MAGIC)
		return false;
	spinlock_acquire_save(&obj->lock);
	*pf = obj->cached_pflags = mi.p_flags;
	atomic_thread_fence(memory_order_seq_cst);
	obj->flags |= OF_CPF_VALID;
	spinlock_release_restore(&obj->lock);
	return true;
}

objid_t obj_compute_id(struct object *obj)
{
	void *kaddr = obj_get_kaddr(obj);
	struct metainfo *mi = (void *)((char *)kaddr + (OBJ_MAXSIZE - OBJ_METAPAGE_SIZE));

	_Alignas(16) blake2b_state S;
	blake2b_init(&S, 32);
	blake2b_update(&S, &mi->nonce, sizeof(mi->nonce));
	blake2b_update(&S, &mi->p_flags, sizeof(mi->p_flags));
	blake2b_update(&S, &mi->kuid, sizeof(mi->kuid));
	size_t tl = 0;
	atomic_thread_fence(memory_order_seq_cst);
	if(mi->p_flags & MIP_HASHDATA) {
		for(size_t s = 0; s < mi->sz; s += mm_page_size(0)) {
			size_t rem = mm_page_size(0);
			if(s + mm_page_size(0) > mi->sz) {
				rem = mi->sz - s;
			}
			assert(rem <= mm_page_size(0));

			void *addr = (char *)kaddr + s + OBJ_NULLPAGE_SIZE;
			blake2b_update(&S, addr, rem);
			tl += rem;
		}
		size_t mdbottom = OBJ_METAPAGE_SIZE + sizeof(struct fotentry) * mi->fotentries;
		size_t pos = OBJ_MAXSIZE - (OBJ_NULLPAGE_SIZE + mdbottom);
		size_t thispage = mm_page_size(0);
		for(size_t s = pos; s < OBJ_MAXSIZE - OBJ_NULLPAGE_SIZE; s += thispage) {
			size_t offset = pos % mm_page_size(0);
			size_t len = mm_page_size(0) - offset;
			void *addr = (char *)kaddr + s + OBJ_NULLPAGE_SIZE;
			blake2b_update(&S, (char *)addr, len);
			tl += len;
		}
	}

	unsigned char tmp[32];
	blake2b_final(&S, tmp, 32);
	_Alignas(16) unsigned char out[16];
	for(int i = 0; i < 16; i++) {
		out[i] = tmp[i] ^ tmp[i + 16];
	}

	obj_release_kaddr(obj);

#if 0
	printk("computed ID tl=%ld " IDFMT " for object " IDFMT "\n",
	  tl,
	  IDPR(*(objid_t *)out),
	  IDPR(obj->id));
#endif
	return *(objid_t *)out;
}

bool obj_verify_id(struct object *obj, bool cache_result, bool uncache)
{
	/* avoid the lock here when checking for KERNEL because this flag is set during creation and
	 * never unset */
	if(obj->id == 0 || (obj->flags & OF_KERNEL))
		return true;
	bool result = false;
	spinlock_acquire_save(&obj->lock);

	if(obj->flags & OF_IDSAFE) {
		result = !!(obj->flags & OF_IDCACHED);
	} else {
		spinlock_release_restore(&obj->lock);
		objid_t c = obj_compute_id(obj);
		spinlock_acquire_save(&obj->lock);
		result = c == obj->id;
		obj->flags |= result && cache_result ? OF_IDCACHED : 0;
	}
	if(uncache) {
		obj->flags &= ~OF_IDSAFE;
	} else {
		obj->flags |= OF_IDSAFE;
	}
	spinlock_release_restore(&obj->lock);
	return result;
}
