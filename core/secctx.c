#include <object.h>
#include <page.h>
#include <processor.h>
#include <secctx.h>
#include <slab.h>
#include <thread.h>
#include <twz/_sctx.h>
#include <twz/_sys.h>

//#define EPRINTK(...) printk(__VA_ARGS__)
#define EPRINTK(...)
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

DECLARE_SLABCACHE(sc_sc, sizeof(struct sctx), _sc_ctor, _sc_dtor, NULL);

struct sctx *secctx_alloc(objid_t repr)
{
	struct sctx *s = slabcache_alloc(&sc_sc);
	krc_init(&s->refs);
	s->repr = repr;
	s->superuser = false;
	return s;
}

void secctx_free(struct sctx *s)
{
	return slabcache_free(s);
}

static void __secctx_krc_put(void *_sc)
{
	struct sctx *sc = _sc;
	(void)sc;
	/* TODO */
}

/* TODO: cache results for faster future lookups */

#include <tomcrypt.h>
#include <tommath.h>
#include <twz/_key.h>
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
	// return true; // TODO
	hash_state hs;
	unsigned char hash[64];
	size_t hashlen;
	switch(htype) {
		case SCHASH_SHA1:
			sha1_init(&hs);
			sha1_process(&hs, (unsigned char *)item, ilen);
			if(data)
				sha1_process(&hs, (unsigned char *)data, dlen);
			sha1_done(&hs, hash);
			hashlen = 20;
			break;
		default:
			return false;
	}

	struct object *to = obj_lookup(target, OBJ_LOOKUP_HIDDEN);
	struct metainfo mi;
	obj_read_data(to, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
	obj_put(to);
	if(mi.magic != MI_MAGIC) {
		printk("MAGIC FAILED\n");
	}

	if(!mi.kuid) {
		/* TODO: should we know this earlier? */
		return false;
	}
	struct object *ko = obj_lookup(mi.kuid, OBJ_LOOKUP_HIDDEN);
	// printk("VERIFY via " IDFMT ": %p\n", IDPR(mi.kuid), ko);

	if(!ko) {
		EPRINTK("COULD NOT LOCATE KU OBJ " IDFMT "\n", IDPR(mi.kuid));
		return false;
	}

	struct key_hdr *hdr = obj_get_kbase(ko);
	if(hdr->type != etype) {
		EPRINTK("hdr->type != cap->etype\n");
		obj_release_kaddr(ko);
		obj_put(ko);
		return false;
	}

	/* TODO: keydata??? */
	void *kd = (char *)hdr + sizeof(*hdr);
	char *k = kd;
	char *nl = strnchr(k, '\n', 4096 /* TODO */);
	char *end = strnchr(nl, '-', 4096 /* TODO */);
	nl++;
	size_t sz = end - nl;
	k = nl;

	// unsigned char keydata[4096];
	/* TODO: better alloc */
	unsigned char *keydata = (void *)mm_memory_alloc(0x1000, PM_TYPE_DRAM, false);
	size_t kdout = 4096;
	bool ret = true;
	int e;
	if((e = base64_decode(k, sz, keydata, &kdout)) != CRYPT_OK) {
		printk("base64 decode error: %s\n", error_to_string(e));
		ret = false;
		obj_release_kaddr(ko);
		obj_put(ko);
		goto done;
	}

	obj_release_kaddr(ko);
	obj_put(ko);

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

	switch(etype) {
		case SCENC_DSA:
			if((e = dsa_import(keydata, kdout, &dk)) != CRYPT_OK) {
				printk("dsa import error: %s\n", error_to_string(e));
				ret = false;
				dsa_free(&dk);
				break;
			}
			/* TODO */
			ret = true;

			dsa_free(&dk);
			goto done;
			return true;

			int stat = 0;
			if((e = dsa_verify_hash((unsigned char *)sig, slen, hash, hashlen, &stat, &dk))
			   != CRYPT_OK) {
				printk("dsa verify error: %s\n", error_to_string(e));
				ret = false;
				break;
			}
			if(!stat) {
				EPRINTK("verification failed\n");
				ret = false;
			}
			break;
		default:
			ret = false;
			break;
	}
done:
	mm_memory_dealloc(keydata);
	// obj_put_page(p);
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
  objid_t target,
  char *sig)
{
	EPRINTK("    - lookup perm cap sc=" IDFMT " : accessor=" IDFMT "\n",
	  IDPR(obj->id),
	  IDPR(cap->accessor));
	if(cap->accessor != obj->id) {
		EPRINTK("wrong accessor\n");
		return 0;
	}
	if(cap->target != target) {
		EPRINTK("wrong target\n");
		return 0;
	}
	if(!__verify_cap(cap, sig)) {
		EPRINTK("CAP verify failed\n");
		return 0;
	}
	/* TODO: verify CAP, rev */

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
  objid_t target,
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
	//= (void *)(dlg + 1);
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
  objid_t target)
{
	uintptr_t off = (uintptr_t)b->data;

	struct sccap *cap;

	char *kaddr = obj_get_kaddr(obj);

	cap = (void *)(kaddr + off);
	if(cap->magic == SC_CAP_MAGIC) {
		if(cap->slen > 4096) {
			EPRINTK("CAP length too long\n");
			/* TODO */
			return 0;
		}
		char *data = kaddr + off + sizeof(struct sccap);
		return __lookup_perm_cap(obj, cap, gs, target, data);
	} else if(cap->magic == SC_DLG_MAGIC) {
		struct scdlg *dlg;
		dlg = (void *)cap;
		size_t rem = dlg->slen + dlg->dlen;
		if(rem > 4096) {
			EPRINTK("DLG length too long\n");
			/* TODO */
			return 0;
		}
		/* TODO: bounds checking on data + lenth of data */
		char *data = kaddr + off + sizeof(struct scdlg);
		return __lookup_perm_dlg(obj, (void *)cap, gs, target, data);
	} else {
		EPRINTK("error - invalid perm object magic number\n");
		return 0;
	}
}

static bool __in_gate(struct scgates *gs, size_t off)
{
	return off >= gs->offset && off < (gs->offset + gs->length)
	       && (off & ((1 << gs->align) - 1)) == 0;
}

static int __lookup_perms(struct object *obj,
  objid_t target,
  size_t ipoff,
  uint32_t *p,
  bool *ingate)
{
	char *kbase = obj_get_kbase(obj);
	struct secctx *ctx = (void *)kbase;

	uint32_t perms = 0;
	bool gatesok = false;
	size_t slot = target % ctx->nbuckets;
	do {
		struct scbucket *b;
		b = (void *)(kbase + sizeof(*ctx) + sizeof(*b) * slot);

		if(b->target == target) {
			EPRINTK("    - lookup_perms: found!\n");
			struct scgates gs = { 0 };
			perms |= __lookup_perm_bucket(obj, b, &gs, target) & b->pmask;
			if(ipoff) {
				__limit_gates(&gs, &b->gatemask);
				if(__in_gate(&gs, ipoff))
					gatesok = true;
			}
		}

		slot = b->chain;
	} while(slot != 0);
	uint32_t dfl = 0;
	struct object *t = obj_lookup(target, OBJ_LOOKUP_HIDDEN);
	if(t) {
		uint32_t p_flags = 0;
		obj_get_pflags(t, &p_flags);
		obj_put(t);
	}
	if(p)
		*p = perms | dfl;
	if(ingate)
		*ingate = gatesok;

	return 0;
}

/* TODO: we could first see if it's mapped in any security context, thereby allowing us to check a
 * "cached" value of the permissions */
int secctx_check_permissions(struct thread *t, uintptr_t ip, struct object *to, uint64_t flags)
{
	char fls[8];
	int __flt = 0;
	if(flags & SCP_READ) {
		fls[__flt++] = 'r';
	}
	if(flags & SCP_WRITE) {
		fls[__flt++] = 'w';
	}
	if(flags & SCP_EXEC) {
		fls[__flt++] = 'x';
	}
	if(flags & SCP_USE) {
		fls[__flt++] = 'u';
	}
	fls[__flt] = 0;

	(void)fls;
	EPRINTK(
	  "[%ld] secctx_check - ip=%lx, target=" IDFMT ", flags=%s\n", t->id, ip, IDPR(to->id), fls);

	spinlock_acquire_save(&t->sc_lock);
	if(t->active_sc->superuser) {
		spinlock_release_restore(&t->sc_lock);
		return 0;
	}

	if(t->active_sc->repr == 0) {
		return -EACCES;
	}

	struct object *obj;
	uint32_t p;
	obj = obj_lookup(t->active_sc->repr, OBJ_LOOKUP_HIDDEN);
	if(!obj) {
		panic("no repr " IDFMT, IDPR(t->active_sc->repr));
	}
	EPRINTK("  - trying active context (" IDFMT ")\n", IDPR(obj->id));
	__lookup_perms(obj, to->id, 0, &p, NULL);
	EPRINTK("    - p = %x (%lx): %s\n", p, flags, (p & flags) == flags ? "ok" : "FAIL");
	if((p & flags) == flags) {
		spinlock_release_restore(&t->sc_lock);
		obj_put(obj);
		return 0;
	}
	obj_put(obj);

	for(int i = 0; i < MAX_SC; i++) {
		if(!t->attached_scs[i] || t->attached_scs[i] == t->active_sc)
			continue;
		obj = obj_lookup(t->attached_scs[i]->repr, OBJ_LOOKUP_HIDDEN);
		if(!obj) {
			panic("no repr " IDFMT, IDPR(t->attached_scs[i]->repr));
		}
		EPRINTK("  - trying " IDFMT "\n", IDPR(obj->id));
		/* also check ip */
		objid_t id;
		if(!vm_vaddr_lookup((void *)ip, &id, NULL)) {
			obj_put(obj);
			continue;
		}
		struct object *eo = obj_lookup(id, OBJ_LOOKUP_HIDDEN);
		if(!eo) {
			obj_put(obj);
			continue;
		}
		uint32_t ep;
		__lookup_perms(obj, eo->id, 0, &ep, NULL);
		EPRINTK("    - ep: %s\n", (ep & SCP_EXEC) ? "ok" : "FAIL");
		obj_put(eo);
		if(!(ep & SCP_EXEC)) {
			obj_put(obj);
			continue;
		}

		__lookup_perms(obj, to->id, 0, &p, NULL);
		EPRINTK(
		  "    - p = %x (%lx): %s\n", p, flags, (p & flags) == flags ? "ok -- SWITCH" : "FAIL");
		if((p & flags) == flags) {
			spinlock_release_restore(&t->sc_lock);
			if(t == current_thread)
				secctx_switch(i);
			obj_put(obj);
			return 0;
		}
		obj_put(obj);
	}
	spinlock_release_restore(&t->sc_lock);
	return -EACCES;
}

int secctx_fault_resolve(struct thread *t,
  uintptr_t ip,
  uintptr_t loaddr,
  uintptr_t vaddr,
  objid_t target,
  uint32_t flags,
  uint64_t *perms)
{
	uint32_t needed = 0;
	char fls[8];
	int __flt = 0;
	if(flags & OBJSPACE_FAULT_READ) {
		fls[__flt++] = 'r';
		needed |= SCP_READ;
	}
	if(flags & OBJSPACE_FAULT_WRITE) {
		fls[__flt++] = 'w';
		needed |= SCP_WRITE;
	}
	if(flags & OBJSPACE_FAULT_EXEC) {
		fls[__flt++] = 'x';
		needed |= SCP_EXEC;
	}
	fls[__flt] = 0;

	(void)fls;
	(void)loaddr;
	EPRINTK("[%ld] fault_resolve - loaddr=%lx, vaddr=%lx, ip=%lx, target=" IDFMT ", flags=%s\n",
	  t->id,
	  loaddr,
	  vaddr,
	  ip,
	  IDPR(target),
	  fls);
	spinlock_acquire_save(&t->sc_lock);
	if(t->active_sc->superuser) {
		spinlock_release_restore(&t->sc_lock);
		*perms = OBJSPACE_EXEC_U | OBJSPACE_WRITE | OBJSPACE_READ;
		return 0;
	}

	if(t->active_sc->repr == 0) {
		goto fault_noperm;
	}

	struct object *obj;
	uint32_t p;
	obj = obj_lookup(t->active_sc->repr, OBJ_LOOKUP_HIDDEN);
	if(!obj) {
		panic("no repr " IDFMT, IDPR(t->active_sc->repr));
	}
	EPRINTK("  - trying active context (" IDFMT ")\n", IDPR(obj->id));
	__lookup_perms(obj, target, 0, &p, NULL);
	EPRINTK("    - p = %x (%x): %s\n", p, needed, (p & needed) == needed ? "ok" : "FAIL");
	if((p & needed) == needed) {
		spinlock_release_restore(&t->sc_lock);

		if(p & SCP_READ) {
			*perms |= OBJSPACE_READ;
		}
		if(p & SCP_WRITE) {
			*perms |= OBJSPACE_WRITE;
		}
		if(p & SCP_EXEC) {
			*perms |= OBJSPACE_EXEC_U;
		}
		obj_put(obj);
		return 0;
	}
	obj_put(obj);

	for(int i = 0; i < MAX_SC; i++) {
		if(!t->attached_scs[i] || t->attached_scs[i] == t->active_sc)
			continue;
		obj = obj_lookup(t->attached_scs[i]->repr, OBJ_LOOKUP_HIDDEN);
		if(!obj) {
			panic("no repr " IDFMT, IDPR(t->attached_scs[i]->repr));
		}
		EPRINTK("  - trying " IDFMT "\n", IDPR(obj->id));
		size_t ipoff = 0;
		bool gok;
		if(flags & OBJSPACE_FAULT_EXEC) {
			assert(ip == vaddr);
			ipoff = ip % OBJ_MAXSIZE;
		}
		/* also check ip */
		objid_t id;
		if(!vm_vaddr_lookup((void *)ip, &id, NULL)) {
			obj_put(obj);
			continue;
		}
		struct object *eo = obj_lookup(id, OBJ_LOOKUP_HIDDEN);
		if(!eo) {
			obj_put(obj);
			continue;
		}
		uint32_t ep;
		__lookup_perms(obj, eo->id, 0, &ep, NULL);
		EPRINTK("    - ep: %s\n", (ep & SCP_EXEC) ? "ok" : "FAIL");
		obj_put(eo);
		if(!(ep & SCP_EXEC)) {
			obj_put(obj);
			continue;
		}

		__lookup_perms(obj, target, ipoff, &p, &gok);
		EPRINTK("    - p = %x (%x): %s (%d)\n",
		  p,
		  needed,
		  (p & needed) == needed ? "ok -- SWITCH" : "FAIL",
		  gok);
		if(((p & needed) == needed) && gok) {
			spinlock_release_restore(&t->sc_lock);

			if(p & SCP_READ) {
				*perms |= OBJSPACE_READ;
			}
			if(p & SCP_WRITE) {
				*perms |= OBJSPACE_WRITE;
			}
			if(p & SCP_EXEC) {
				*perms |= OBJSPACE_EXEC_U;
			}
			if(t == current_thread)
				secctx_switch(i);
			obj_put(obj);
			return 0;
		}
		obj_put(obj);
	}
fault_noperm:
	spinlock_release_restore(&t->sc_lock);
	*perms = 0;
	struct fault_sctx_info info = {
		.ip = ip,
		.addr = ip,
		.target = target,
		.pneed = needed,
	};
	thread_raise_fault(t, FAULT_SCTX, &info, sizeof(info));

	return -1;
}

#undef EPRINTK
#define EPRINTK(...) printk(__VA_ARGS__)

static void __secctx_update_thrdrepr(struct thread *thr, int s, bool at)
{
	struct object *to = kso_get_obj(thr->throbj, thr);
	struct kso_attachment k = {
		.id = at ? thr->attached_scs[s]->repr : 0,
		.flags = 0,
		.info = at ? thr->attached_scs_attrs[s] : 0,
		.type = KSO_SECCTX,
	};
	obj_write_data(to, offsetof(struct twzthread_repr, attached) + sizeof(k) * s, sizeof(k), &k);
	obj_put(to);
}

static bool secctx_thread_attach(struct sctx *s, struct thread *t)
{
	// EPRINTK("thread %ld attach to " IDFMT "\n", t->id, IDPR(s->repr));
	bool ok = false, found = false, force = false;
	size_t i;
	spinlock_acquire_save(&t->sc_lock);
	if(t->active_sc->repr == 0) {
		/* bootstrap context. Get rid of it, we're a real security context now! */
		assert(t->attached_scs[0] == t->active_sc);
		t->attached_scs[0] = NULL;
		/* once for attached[0] */
		krc_put_call(t->active_sc, refs, __secctx_krc_put);
		/* and again for the active_sc */
		force = true;
	}

	for(i = 0; i < MAX_SC; i++) {
		if(t->attached_scs[i] == s) {
			t->attached_scs_attrs[i] = 0;
			__secctx_update_thrdrepr(t, i, true);
			found = true;
			break;
		}
	}
	if(!found) {
		for(i = 0; i < MAX_SC; i++) {
			if(t->attached_scs[i] == NULL) {
				krc_get(&s->refs);
				t->attached_scs[i] = s;
				t->attached_scs_attrs[i] = 0;
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
	// EPRINTK("thread %ld detach from " IDFMT "\n", thr->id, IDPR(s->repr));
	if(s->repr == 0) {
		printk("THIS IS TOTALLY A BUG THAT NEEDS TO BE SOLVED!\n\n!!!!!!!!!!\n!!!!!!!\n!!!!!!\n");
	}
	bool ok = false;
	ssize_t na = -1;
	for(size_t i = 0; i < MAX_SC; i++) {
		if(thr->attached_scs[i] == s) {
			__secctx_update_thrdrepr(thr, i, false);
			krc_put_call(s, refs, __secctx_krc_put);
			thr->attached_scs[i] = NULL;
			thr->attached_scs_attrs[i] = 0;
			ok = true;
		} else if(na == -1 && thr->attached_scs[i]) {
			na = i;
		}
	}
	if(na == -1) {
		/* detached from the last context. Create a dummy context */
		assert(thr->attached_scs[0] == NULL);
		thr->active_sc = secctx_alloc(0);
		krc_get(&thr->active_sc->refs);
		thr->attached_scs[0] = thr->active_sc;
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
		uint16_t as = thr->attached_scs_attrs[i] >> 16;
		uint16_t flags = thr->attached_scs_attrs[i] & 0xffff;
		if(!(flags & __TWZ_DETACH_DETACH)) {
			continue;
		}
		bool onen = !!(flags & TWZ_DETACH_ONENTRY);
		bool onex = !!(flags & TWZ_DETACH_ONEXIT);
		if(thr->attached_scs[i] && as == sysc) {
			if(((entry && onen) || (!entry && onex))) {
				__secctx_thread_detach(thr->attached_scs[i], thr);
			}
		}
		if(!onen && !onex) {
			assert(!onen && !onex);
			if(entry && thr->attached_scs[i]) {
				krc_get(&thr->attached_scs[i]->refs);
				thr->attached_scs_backup[i] = thr->attached_scs[i];
				thr->attached_scs_attrs_backup[i] = thr->attached_scs_attrs[i];
				__secctx_thread_detach(thr->attached_scs[i], thr);
			} else if(!entry && thr->attached_scs_backup[i]) {
				thr->attached_scs[i] = thr->attached_scs_backup[i];
				thr->attached_scs_attrs[i] = thr->attached_scs_attrs_backup[i];
				thr->attached_scs_backup[i] = NULL;
				if(!thr->active_sc->repr) {
					/* once for attached[0] */
					krc_put_call(thr->active_sc, refs, __secctx_krc_put);
					/* and again for the active_sc */
					krc_put_call(thr->active_sc, refs, __secctx_krc_put);

					thr->active_sc = thr->attached_scs[i];
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
			thr->attached_scs_attrs[i] = (flags & 0xffff) | (sysc << 16) | __TWZ_DETACH_DETACH;
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
		if(thr->attached_scs[i] == s) {
			uint16_t oldflags = thr->attached_scs_attrs[i] & 0xffff;
			thr->attached_scs_attrs[i] =
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
	o->sctx.sc = secctx_alloc(o->id);
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
