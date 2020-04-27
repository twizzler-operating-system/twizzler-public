#include <kalloc.h>
#include <object.h>
#include <page.h>
#include <processor.h>
#include <secctx.h>
#include <slab.h>
#include <thread.h>
#include <twz/_sctx.h>
#include <twz/_sys.h>

#define EPRINTK(...) printk(__VA_ARGS__)
//#define EPRINTK(...)
static void _sc_ctor(void *_x __unused, void *ptr)
{
	struct sctx *sc = ptr;
	object_space_init(&sc->space);
}

static void _sc_dtor(void *_x __unused, void *ptr)
{
	struct sctx *sc = ptr;
	object_space_destroy(&sc->space);
}

static DECLARE_SLABCACHE(sc_sc, sizeof(struct sctx), _sc_ctor, _sc_dtor, NULL);
static DECLARE_SLABCACHE(sc_sctx_ce, sizeof(struct sctx_cache_entry), NULL, NULL, NULL);

struct sctx *secctx_alloc(struct object *obj)
{
	struct sctx *s = slabcache_alloc(&sc_sc);
	krc_init(&s->refs);
	if(obj)
		krc_get(&obj->refs);
	s->obj = obj;
	s->superuser = false;
	return s;
}

void secctx_free(struct sctx *s)
{
	return slabcache_free(&sc_sc, s);
}

static void __secctx_krc_put(void *_sc)
{
	struct sctx *sc = _sc;
	if(sc->obj) {
		obj_put(sc->obj);
		sc->obj = NULL;
	}

	struct rbnode *next;
	for(struct rbnode *node = rb_first(&sc->cache); node; node = next) {
		struct sctx_cache_entry *scce = rb_entry(node, struct sctx_cache_entry, node);
		if(scce->gates) {
			kfree(scce->gates);
		}
		next = rb_next(node);
		rb_delete(&scce->node, &sc->cache);
		slabcache_free(&sc_sctx_ce, scce);
	}
}

static int __sctx_ce_compar_key(struct sctx_cache_entry *a, objid_t b)
{
	if(a->id > b)
		return 1;
	else if(a->id < b)
		return -1;
	return 0;
}

static int __sctx_ce_compar(struct sctx_cache_entry *a, struct sctx_cache_entry *b)
{
	return __sctx_ce_compar_key(a, b->id);
}

static void sctx_cache_delete(struct sctx *sc, objid_t id)
{
	printk("[sctx] cache try-delete " IDFMT "\n", IDPR(id));
	struct rbnode *node =
	  rb_search(&sc->cache, id, struct sctx_cache_entry, node, __sctx_ce_compar_key);
	if(node) {
		printk("[sctx] cache delete " IDFMT "\n", IDPR(id));
		struct sctx_cache_entry *scce = rb_entry(node, struct sctx_cache_entry, node);
		if(scce->gates) {
			kfree(scce->gates);
		}
		slabcache_free(&sc_sctx_ce, scce);
	}
}

static struct sctx_cache_entry *sctx_cache_lookup(struct sctx *sc, objid_t id)
{
	printk("[sctx] cache lookup " IDFMT "\n", IDPR(id));
	struct rbnode *node =
	  rb_search(&sc->cache, id, struct sctx_cache_entry, node, __sctx_ce_compar_key);
	struct sctx_cache_entry *scce = node ? rb_entry(node, struct sctx_cache_entry, node) : NULL;
	if(scce) {
		printk("[sctx] cache found " IDFMT ": %x (%ld gates)\n",
		  IDPR(id),
		  scce->perms,
		  scce->gate_count);
	}
	return scce;
}

static void sctx_cache_insert(struct sctx *sc,
  objid_t id,
  uint32_t perms,
  struct scgates *gates,
  size_t gc)
{
	printk("[sctx] cache insert " IDFMT ": %x (%ld gates)\n", IDPR(id), perms, gc);
	struct sctx_cache_entry *ce = slabcache_alloc(&sc_sctx_ce);
	ce->id = id;
	ce->perms = perms;
	ce->gates = gates;
	ce->gate_count = gc;
	rb_insert(&sc->cache, ce, struct sctx_cache_entry, node, __sctx_ce_compar);
}

#include <tomcrypt.h>
#include <tommath.h>
#include <twz/_key.h>

static ssize_t __verify_get_hash(uint32_t htype,
  void *item,
  void *data,
  size_t ilen,
  size_t dlen,
  unsigned char *out)
{
	hash_state hs;
	switch(htype) {
		case SCHASH_SHA1:
			sha1_init(&hs);
			sha1_process(&hs, (unsigned char *)item, ilen);
			if(data)
				sha1_process(&hs, (unsigned char *)data, dlen);
			sha1_done(&hs, out);
			return 20;
		default:
			return -1;
	}
}

static objid_t __verify_get_object_kuid(objid_t target)
{
	struct metainfo mi;
	struct object *to = obj_lookup(target, OBJ_LOOKUP_HIDDEN);
	obj_read_data(to, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	if(!obj_verify_id(to, true, false)) {
		obj_put(to);
		return 0;
	}
	obj_put(to);
	if(mi.magic != MI_MAGIC) {
		return 0;
	}
	if(!mi.kuid) {
		return 0;
	}
	return mi.kuid;
}

static unsigned char *__verify_load_keydata(struct object *ko, uint32_t etype, size_t *kdout)
{
	struct key_hdr *hdr = obj_get_kbase(ko);
	if(hdr->type != etype) {
		EPRINTK("hdr->type != cap->etype\n");
		obj_release_kaddr(ko);
		return NULL;
	}

	/* TODO: actually look at the data pointer, because this sucks lol */
	void *kd = (char *)hdr + sizeof(*hdr);
	char *k = kd;
	char *nl = strnchr(k, '\n', 4096 /* TODO */);
	char *end = strnchr(nl, '-', 4096 /* TODO */);
	nl++;
	size_t sz = end - nl;
	k = nl;

	/* note - base64 means the output data will be smaller than the input, so we could save space
	 * here. */
	unsigned char *keydata = kalloc(sz);
	*kdout = sz;
	int e;
	if((e = base64_decode(k, sz, keydata, kdout)) != CRYPT_OK) {
		EPRINTK("base64 decode error: %s\n", error_to_string(e));
		obj_release_kaddr(ko);
		kfree(keydata);
		keydata = NULL;
	}

	obj_release_kaddr(ko);
	return keydata;
}

static bool __verify_region(void *item,
  void *data,
  char *sig,
  size_t ilen,
  size_t dlen,
  size_t slen,
  uint32_t htype,
  uint32_t etype,
  objid_t target)
{
	unsigned char hash[64];
	ssize_t hashlen = __verify_get_hash(htype, item, data, ilen, dlen, hash);
	if(hashlen < 0) {
		return false;
	}

	objid_t kuid = __verify_get_object_kuid(target);
	struct object *ko = obj_lookup(kuid, OBJ_LOOKUP_HIDDEN);
	if(!ko) {
		EPRINTK("COULD NOT LOCATE KU OBJ " IDFMT "\n", IDPR(kuid));
		return false;
	}

	size_t kdout;
	unsigned char *keydata = __verify_load_keydata(ko, etype, &kdout);
	obj_put(ko);
	if(!keydata) {
		return false;
	}

	/*
	printk("SIG: ");
	for(int i = 0; i < cap->slen; i++) {
	    printk("%x ", (unsigned char)sig[i]);
	}

	printk("\nHASH (%ld): ", hashlen);
	for(unsigned int i = 0; i < hashlen; i++) {
	    printk("%x ", (unsigned char)hash[i]);
	}
	printk("\n");
	*/

	dsa_key dk;
	ltc_mp = ltm_desc;

	int e;
	bool ret = false;
	switch(etype) {
		case SCENC_DSA:
			if((e = dsa_import(keydata, kdout, &dk)) != CRYPT_OK) {
				printk("dsa import error: %s\n", error_to_string(e));
				dsa_free(&dk);
				break;
			}
			int stat = 0;
			if((e = dsa_verify_hash((unsigned char *)sig, slen, hash, hashlen, &stat, &dk))
			   != CRYPT_OK) {
				printk("dsa verify error: %s\n", error_to_string(e));
				dsa_free(&dk);
				break;
			}
			dsa_free(&dk);
			if(!stat) {
				EPRINTK("verification failed\n");
				ret = false;
			}
			ret = true;
			break;
		default:
			ret = false;
			break;
	}
	kfree(keydata);
	return ret;
}

static bool __verify_cap(struct sccap *cap, char *sig)
{
	return __verify_region(
	  cap, NULL, sig, sizeof(*cap), 0, cap->slen, cap->htype, cap->etype, cap->target);
}

static bool __verify_dlg(struct scdlg *dlg, char *data)
{
	return __verify_region(dlg,
	  data,
	  data + dlg->dlen,
	  sizeof(*dlg),
	  dlg->dlen,
	  dlg->slen,
	  dlg->htype,
	  dlg->etype,
	  dlg->delegator);
	return true;
}

static uint32_t __lookup_perm_cap(struct object *obj,
  struct sccap *cap,
  struct scgates *gs,
  struct object *target,
  char *sig)
{
	EPRINTK("    - lookup perm cap sc=" IDFMT " : accessor=" IDFMT "\n",
	  IDPR(obj->id),
	  IDPR(cap->accessor));
	if(cap->accessor != obj->id) {
		EPRINTK("wrong accessor\n");
		return 0;
	}
	if(cap->target != target->id) {
		EPRINTK("wrong target\n");
		return 0;
	}
	if(!__verify_cap(cap, sig)) {
		EPRINTK("CAP verify failed\n");
		return 0;
	}
	if(cap->flags & SCF_GATE)
		*gs = cap->gates;
	return cap->perms;
}

static void __limit_gates(struct scgates *gs, struct scgates *m)
{
	/* TODO: check overflow on all calculations */
	size_t end = gs->offset + gs->length;
	size_t start = gs->offset > m->offset ? gs->offset : m->offset;
	if(start >= end) {
		gs->offset = 0;
		gs->length = 0;
		return;
	}
	end = start + (gs->length > m->length ? m->length : gs->length);
	if(end <= gs->offset) {
		gs->offset = 0;
		gs->length = 0;
		return;
	}

	gs->offset = start;
	gs->length = end - start;
	gs->align = gs->align > m->align ? gs->align : m->align;
}

static uint32_t __lookup_perm_dlg(struct object *obj,
  struct scdlg *dlg,
  struct scgates *gs,
  struct object *target,
  char *data)
{
	if(dlg->delegatee != obj->id) {
		EPRINTK("wrong delegatee\n");
		return 0;
	}

	if(!__verify_dlg(dlg, data)) {
		EPRINTK("DLG verify failed\n");
		return 0;
	}
	struct sccap *next = (struct sccap *)data;
	uint32_t p = 0;
	if(next->magic == SC_CAP_MAGIC) {
		if(next->accessor != dlg->delegator) {
			EPRINTK("broken chain\n");
			return 0;
		}
		p = __lookup_perm_cap(obj, next, gs, target, data + sizeof(struct sccap));
	} else if(next->magic == SC_DLG_MAGIC) {
		struct scdlg *nd = (void *)data;
		if(nd->delegatee != dlg->delegator) {
			EPRINTK("broken chain\n");
			return 0;
		}
		p = __lookup_perm_dlg(obj, nd, gs, target, data + sizeof(struct scdlg));
	} else {
		EPRINTK("invalid magic in chain\n");
	}
	if(!(p & SCP_CD)) {
		EPRINTK("chain is not CD\n");
		return 0;
	}
	if(dlg->flags & SCF_GATE) {
		__limit_gates(gs, &dlg->gates);
	}
	return p & dlg->mask;
}

static uint32_t __lookup_perm_bucket(struct object *obj,
  struct scbucket *b,
  struct scgates *gs,
  struct object *target)
{
	uintptr_t off = (uintptr_t)b->data;

	struct sccap *cap;

	char *kaddr = obj_get_kaddr(obj);

	cap = (void *)(kaddr + off);
	uint32_t ret = 0;
	if(!obj_kaddr_valid(obj, cap, sizeof(*cap))) {
		printk("[sctx]: warning - invalid offset specified for bucket\n");
	} else {
		if(cap->magic == SC_CAP_MAGIC) {
			char *data = kaddr + off + sizeof(struct sccap);
			/* TODO: overflow */
			if(!obj_kaddr_valid(obj, cap, sizeof(*cap) + cap->slen)) {
				printk("[sctx]: warning - invalid cap length (%ld) in context " IDFMT
				       " for target " IDFMT "\n",
				  cap->slen + sizeof(*cap),
				  IDPR(obj->id),
				  IDPR(target->id));
			} else {
				ret = __lookup_perm_cap(obj, cap, gs, target, data);
			}
		} else if(cap->magic == SC_DLG_MAGIC) {
			struct scdlg *dlg;
			dlg = (void *)cap;
			/* TODO: overflow */
			size_t rem = dlg->slen + dlg->dlen;
			/* TODO: overflow */
			if(!obj_kaddr_valid(obj, dlg, sizeof(*dlg) + rem)) {
				printk("[sctx]: warning - invalid dlg length (%ld) in context " IDFMT
				       " for target " IDFMT "\n",
				  rem + sizeof(*dlg),
				  IDPR(obj->id),
				  IDPR(target->id));
			} else {
				char *data = kaddr + off + sizeof(struct scdlg);
				ret = __lookup_perm_dlg(obj, (void *)cap, gs, target, data);
			}
		} else {
			printk("[sctx]: warning - invalid context entry magic number\n");
		}
	}

	obj_release_kaddr(obj);
	return ret;
}

static bool __in_gate(struct scgates *gs, size_t off)
{
	return off >= gs->offset && off < (gs->offset + gs->length)
	       && (off & ((1 << gs->align) - 1)) == 0;
}

static void __append_gatelist(struct scgates **gl, size_t *count, size_t *pos, struct scgates *gate)
{
	if(*pos == *count) {
		*count = *count ? 1 : *count * 2;
		*gl = krealloc(*gl, sizeof(**gl) * (*count));
	}

	(*gl)[*pos] = *gate;
	(*pos)++;
}
/* given a security context, obj, and a target object, lookup the permissions that this context
 * has for accessing this object. This is a logical or of all the capabilities and delegations
 * for the target in this context.
 *
 * If ipoff is non-zero, also check if ipoff exists within a gate of this object. If any cap
 * provides a gate that matches ipoff, it's ok. */
static void __lookup_perms(struct sctx *sc,
  struct object *target,
  size_t ipoff,
  uint32_t *p,
  bool *ingate)
{
	/* first try the cache */
	spinlock_acquire_save(&sc->cache_lock);
	struct sctx_cache_entry *scce = sctx_cache_lookup(sc, target->id);
	if(scce) {
		*p = scce->perms;
		if(ingate) {
			*ingate = false;
			for(size_t i = 0; i < scce->gate_count; i++) {
				*ingate = *ingate || __in_gate(&scce->gates[i], ipoff);
			}
		}
		spinlock_release_restore(&sc->cache_lock);
		return;
	}
	spinlock_release_restore(&sc->cache_lock);
	char *kbase = obj_get_kbase(sc->obj);
	struct secctx *ctx = (void *)kbase;

	uint32_t perms = 0;
	bool gatesok = ipoff == 0;
	size_t slot = target->id % ctx->nbuckets;
	struct scgates *gatelist = NULL;
	size_t gatecount = 0, gatepos = 0;
	do {
		struct scbucket *b;
		b = (void *)(kbase + sizeof(*ctx) + sizeof(*b) * slot);
		if(!obj_kaddr_valid(sc->obj, b, sizeof(*b))) {
			break;
		}

		if(b->target == target->id) {
			EPRINTK("    - lookup_perms: found!\n");
			struct scgates gs = { 0 };
			/* get this entry's perm, masked with this buckets mask. Then, if we're gating,
			 * check the gates. */
			perms |= __lookup_perm_bucket(sc->obj, b, &gs, target) & b->pmask;
			/* TODO: only do this if the gate is meaningful */
			__limit_gates(&gs, &b->gatemask);
			__append_gatelist(&gatelist, &gatecount, &gatepos, &gs);
			if(ipoff) {
				/* we first have to limit the gate from the cap by the "gatemask" of the bucket.
				 * This isn't as trivial as a logical and, so see the above function */
				if(__in_gate(&gs, ipoff))
					gatesok = true;
			}
		}

		slot = b->chain;
	} while(slot != 0);
	/* we _also_ need to get the default permissions for the object and take them into account.
	 */
	uint32_t dfl = 0;
	uint32_t p_flags = 0;
	obj_get_pflags(target, &p_flags);
	dfl |= (p_flags & MIP_DFL_READ) ? SCP_READ : 0;
	dfl |= (p_flags & MIP_DFL_WRITE) ? SCP_WRITE : 0;
	dfl |= (p_flags & MIP_DFL_EXEC) ? SCP_EXEC : 0;
	if(p) {
		*p = perms | dfl;
	}
	if(ingate) {
		*ingate = gatesok;
	}

	spinlock_acquire_save(&sc->cache_lock);
	if(sctx_cache_lookup(sc, target->id)) {
		sctx_cache_delete(sc, target->id);
	}
	sctx_cache_insert(sc, target->id, perms | dfl, gatelist, gatepos);
	spinlock_release_restore(&sc->cache_lock);

	obj_release_kaddr(sc->obj);
}

static int check_if_valid(struct sctx *sc,
  void *ip,
  struct object *target,
  uint32_t flags,
  size_t ipoff,
  uint32_t *perms)
{
	/* grab the current executing object according to the instruction pointer */
	objid_t eoid;
	if(!vm_vaddr_lookup(ip, &eoid, NULL)) {
		return -1;
	}
	struct object *eo = obj_lookup(eoid, OBJ_LOOKUP_HIDDEN);
	if(!eo) {
		return -1;
	}
	/* must be executable in this context */
	uint32_t ep;
	__lookup_perms(sc, eo, 0, &ep, NULL);
	obj_put(eo);
	if(!(ep & SCP_EXEC)) {
		return -1;
	}

	/* check the actual target permissions. Note that if we're gating, we need to check that
	 * too. The functions above only check gates if ipoff is non-zero (as this would be a fault
	 * anyway if we tried executing there!) */
	bool gok;
	uint32_t p;
	__lookup_perms(sc, target, ipoff, &p, &gok);
	if(((p & flags) == flags) && gok) {
		if(perms)
			*perms = p;
		return 0;
	}
	return -1;
}

int secctx_fault_resolve(void *ip,
  uintptr_t loaddr,
  void *vaddr,
  struct object *target,
  uint32_t needed,
  uint32_t *perms,
  bool do_fault)
{
	char fls[8];
	int __flt = 0;
	if(needed & SCP_READ) {
		fls[__flt++] = 'r';
	}
	if(needed & SCP_WRITE) {
		fls[__flt++] = 'w';
	}
	if(needed & SCP_EXEC) {
		fls[__flt++] = 'x';
	}
	fls[__flt] = 0;

	(void)fls;
	(void)loaddr;
	EPRINTK("[%ld] fault_resolve - loaddr=%lx, vaddr=%p, ip=%p, target=" IDFMT ", flags=%s\n",
	  current_thread->id,
	  loaddr,
	  vaddr,
	  ip,
	  IDPR(target->id),
	  fls);
	spinlock_acquire_save(&current_thread->sc_lock);
	/* check if we have "superuser" priv. This is only really the init thread before it attaches
	 * to a real security context */
	if(current_thread->active_sc->superuser) {
		spinlock_release_restore(&current_thread->sc_lock);
		if(perms)
			*perms = SCP_READ | SCP_WRITE | SCP_EXEC;
		return 0;
	}

	/* we have a "temporary" context with no actual backing. Thus there are no permissions. */
	if(current_thread->active_sc->obj == NULL) {
		goto fault_noperm;
	}

	/* try out the active context and see if that's enough. As an optimization, we don't have to
	 * check gates or if the ip is executable, since it must be */
	uint32_t p;
	EPRINTK("  - trying active context (" IDFMT ")\n",
	  IDPR(current_thread->active_sc->obj ? current_thread->active_sc->obj->id : (objid_t)0));
	__lookup_perms(current_thread->active_sc, target, 0, &p, NULL);
	EPRINTK("    - p = %x (%x): %s\n", p, needed, (p & needed) == needed ? "ok" : "FAIL");
	if((p & needed) == needed) {
		spinlock_release_restore(&current_thread->sc_lock);
		if(perms)
			*perms = p;
		return 0;
	}

	/* try out the other attached contexts. If a given context is valid for this access, switch
	 * to it. */
	for(int i = 0; i < MAX_SC; i++) {
		struct sctx *sc = current_thread->sctx_entries[i].context;
		if(!sc || sc == current_thread->active_sc)
			continue;
		assert(sc->obj);
		EPRINTK("  - trying " IDFMT "\n", IDPR(sc->obj->id));
		size_t ipoff = 0;
		/* if we need executable permission, then we are jumping into an object, possibly
		 * through a
		 * gate. If so, we need to check if the gates this objects provides match our target IP.
		 */
		if(needed & SCP_EXEC) {
			assert(ip == vaddr);
			ipoff = (uintptr_t)ip % OBJ_MAXSIZE;
		}

		if(check_if_valid(sc, ip, target, needed, ipoff, &p) == 0) {
			spinlock_release_restore(&current_thread->sc_lock);
			EPRINTK("  - Success, switching\n");
			if(perms)
				*perms = p;
			secctx_switch(i);
			return 0;
		}
	}
	/* if we didn't find anything, fall through to here */
fault_noperm:
	spinlock_release_restore(&current_thread->sc_lock);
	if(perms)
		*perms = 0;
	if(do_fault) {
		/* TODO: check if this is right */
		struct fault_sctx_info info =
		  twz_fault_build_sctx_info(target->id, ip, loaddr % OBJ_MAXSIZE, needed);
		thread_raise_fault(current_thread, FAULT_SCTX, &info, sizeof(info));
	}

	return -1;
}

/* TODO: we could first see if it's mapped in any security context, thereby allowing us to check
 * a "cached" value of the permissions */
int secctx_check_permissions(void *ip, struct object *to, uint32_t flags)
{
	if(secctx_fault_resolve(ip, 0, NULL, to, flags, NULL, false)) {
		return -EACCES;
	}
	return 0;
}

#undef EPRINTK
#define EPRINTK(...) printk(__VA_ARGS__)

static void __secctx_update_thrdrepr(struct thread *thr, int s, bool at)
{
	struct object *to = kso_get_obj(thr->throbj, thr);
	struct kso_attachment k = {
		.id = at && thr->sctx_entries[s].context->obj ? thr->sctx_entries[s].context->obj->id : 0,
		.flags = 0,
		.info = at ? thr->sctx_entries[s].attr : 0,
		.type = KSO_SECCTX,
	};
	obj_write_data(to, offsetof(struct twzthread_repr, attached) + sizeof(k) * s, sizeof(k), &k);
	obj_put(to);
}

static bool secctx_thread_attach(struct sctx *s, struct thread *t)
{
	// EPRINTK("thread %ld attach to " IDFMT "\n", t->id, IDPR(s->obj->id));
	bool ok = false, found = false, force = false;
	size_t i;
	spinlock_acquire_save(&t->sc_lock);
	if(t->active_sc->obj == NULL) {
		/* bootstrap context. Get rid of it, we're a real security context now! */
		assert(t->sctx_entries[0].context == t->active_sc);
		t->sctx_entries[0].context = NULL;
		/* once for attached[0] */
		krc_put_call(t->active_sc, refs, __secctx_krc_put);
		/* and again for the active_sc */
		force = true;
	}

	for(i = 0; i < MAX_SC; i++) {
		if(t->sctx_entries[i].context == s) {
			t->sctx_entries[i].attr = 0;
			__secctx_update_thrdrepr(t, i, true);
			found = true;
			break;
		}
	}
	if(!found) {
		for(i = 0; i < MAX_SC; i++) {
			if(t->sctx_entries[i].context == NULL) {
				krc_get(&s->refs);
				t->sctx_entries[i].context = s;
				t->sctx_entries[i].attr = 0;
				__secctx_update_thrdrepr(t, i, true);
				ok = true;
				break;
			}
		}
		if(ok && force) {
			struct sctx *ac = t->active_sc;
			secctx_switch(i);
			krc_put_call(ac, refs, __secctx_krc_put);
		}
	}
	spinlock_release_restore(&t->sc_lock);
	return ok;
}

static bool __secctx_thread_detach(struct sctx *s, struct thread *thr)
{
	// EPRINTK("thread %ld detach from " IDFMT "\n", thr->id, IDPR(s->obj->id));
	bool ok = false;
	ssize_t na = -1;
	for(size_t i = 0; i < MAX_SC; i++) {
		if(thr->sctx_entries[i].context == s) {
			__secctx_update_thrdrepr(thr, i, false);
			krc_put_call(s, refs, __secctx_krc_put);
			thr->sctx_entries[i].context = NULL;
			thr->sctx_entries[i].attr = 0;
			ok = true;
		} else if(na == -1 && thr->sctx_entries[i].context) {
			na = i;
		}
	}
	if(na == -1) {
		/* detached from the last context. Create a dummy context */
		assert(thr->sctx_entries[0].context == NULL);
		thr->active_sc = secctx_alloc(NULL);
		krc_get(&thr->active_sc->refs);
		thr->sctx_entries[0].context = thr->active_sc;
		secctx_switch(0);
	} else if(thr->active_sc == s) {
		/* TODO: maybe we could leave this? */
		// thr->active_sc = NULL;
		secctx_switch(na);
	}
	return ok;
}

static bool secctx_thread_detach(struct sctx *s, struct thread *thr)
{
	bool ok;
	spinlock_acquire_save(&thr->sc_lock);
	ok = __secctx_thread_detach(s, thr);
	spinlock_release_restore(&thr->sc_lock);
	return ok;
}

#define __TWZ_DETACH_DETACH 0x1000
static bool __secctx_detach_event(struct thread *thr, bool entry, int sysc)
{
	spinlock_acquire_save(&thr->sc_lock);
	for(size_t i = 0; i < MAX_SC; i++) {
		uint16_t as = thr->sctx_entries[i].attr >> 16;
		uint16_t flags = thr->sctx_entries[i].attr & 0xffff;
		if(!(flags & __TWZ_DETACH_DETACH)) {
			continue;
		}
		bool onen = !!(flags & TWZ_DETACH_ONENTRY);
		bool onex = !!(flags & TWZ_DETACH_ONEXIT);
		if(thr->sctx_entries[i].context && as == sysc) {
			if(((entry && onen) || (!entry && onex))) {
				__secctx_thread_detach(thr->sctx_entries[i].context, thr);
			}
		}
		if(!onen && !onex) {
			assert(!onen && !onex);
			if(entry && thr->sctx_entries[i].context) {
				krc_get(&thr->sctx_entries[i].context->refs);
				thr->sctx_entries[i].backup = thr->sctx_entries[i].context;
				thr->sctx_entries[i].backup_attr = thr->sctx_entries[i].attr;
				__secctx_thread_detach(thr->sctx_entries[i].context, thr);
			} else if(!entry && thr->sctx_entries[i].backup) {
				thr->sctx_entries[i].context = thr->sctx_entries[i].backup;
				thr->sctx_entries[i].attr = thr->sctx_entries[i].backup_attr;
				thr->sctx_entries[i].backup = NULL;
				if(!thr->active_sc->obj) {
					/* once for attached[0] */
					krc_put_call(thr->active_sc, refs, __secctx_krc_put);
					/* and again for the active_sc */
					krc_put_call(thr->active_sc, refs, __secctx_krc_put);

					thr->active_sc = thr->sctx_entries[i].context;
					secctx_switch(i);
				}
			}
		}
	}
	spinlock_release_restore(&thr->sc_lock);
	return true;
}

static bool secctx_detach_all(struct thread *thr, int sysc, int flags)
{
	bool ok = true;
	spinlock_acquire_save(&thr->sc_lock);
	for(size_t i = 0; i < MAX_SC; i++) {
		if(flags) {
			thr->sctx_entries[i].attr = (flags & 0xffff) | (sysc << 16) | __TWZ_DETACH_DETACH;
		}
	}
	spinlock_release_restore(&thr->sc_lock);
	return ok;
}

static bool __secctx_detach(struct object *parent, struct object *child, int sysc, int flags)
{
	/* TODO: actually get the thread object */
	if(!child)
		return secctx_detach_all(current_thread, sysc, flags);
	if(parent->kso_type != KSO_THREAD || child->kso_type != KSO_SECCTX)
		return false;
	struct thread *thr = current_thread;
	struct sctx *s = child->sctx.sc;

	bool ok = false;
	spinlock_acquire_save(&thr->sc_lock);
	for(size_t i = 0; i < MAX_SC; i++) {
		if(thr->sctx_entries[i].context == s) {
			uint16_t oldflags = thr->sctx_entries[i].attr & 0xffff;
			thr->sctx_entries[i].attr =
			  ((flags | oldflags) & 0xffff) | (sysc << 16) | __TWZ_DETACH_DETACH;
			ok = true;
			break;
		}
	}
	spinlock_release_restore(&thr->sc_lock);
	return ok;
}

static bool __secctx_attach(struct object *parent, struct object *child, int flags)
{
	(void)flags;
	if(parent->kso_type != KSO_THREAD || child->kso_type != KSO_SECCTX)
		return false;
	/* TODO: actually get the thread object */
	struct thread *thr = current_thread;
	return secctx_thread_attach(child->sctx.sc, thr);
}

static void __secctx_ctor(struct object *o)
{
	o->sctx.sc = secctx_alloc(o);
}

static struct kso_calls __ksoc_sctx = {
	.attach = __secctx_attach,
	.detach = __secctx_detach,
	.detach_event = __secctx_detach_event,
	.ctor = __secctx_ctor,
};

__initializer static void __init_kso_secctx(void)
{
	kso_register(KSO_SECCTX, &__ksoc_sctx);
}
