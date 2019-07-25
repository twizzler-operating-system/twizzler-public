#include <object.h>
#include <processor.h>
#include <secctx.h>
#include <slab.h>
#include <thread.h>
#include <twz/_sctx.h>
#include <twz/_sys.h>

#define printk(...)
static void _sc_ctor(void *_x __unused, void *ptr)
{
	struct sctx *sc = ptr;
	arch_secctx_init(sc);
}

static void _sc_dtor(void *_x __unused, void *ptr)
{
	struct sctx *sc = ptr;
	arch_secctx_destroy(sc);
}

DECLARE_SLABCACHE(sc_sc, sizeof(struct sctx), _sc_ctor, _sc_dtor, NULL);

struct sctx *secctx_alloc(objid_t repr)
{
	struct sctx *s = slabcache_alloc(&sc_sc);
	krc_init(&s->refs);
	s->repr = repr;
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

static uint32_t __lookup_perm_cap(struct object *obj, struct sccap *cap, struct scgates *gs)
{
	printk("    - lookup perm cap sc=" IDFMT " : accessor=" IDFMT "\n",
	  IDPR(obj->id),
	  IDPR(cap->accessor));
	if(cap->accessor != obj->id) {
		printk("wrong accessor\n");
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

static uint32_t __lookup_perm_dlg(struct object *obj, struct scdlg *dlg, struct scgates *gs)
{
	if(dlg->delegatee != obj->id) {
		printk("wrong delegatee\n");
		return 0;
	}
	struct sccap *next = (void *)(dlg + 1);
	uint32_t p = 0;
	if(next->magic == SC_CAP_MAGIC) {
		if(next->accessor != dlg->delegator) {
			printk("broken chain\n");
			return 0;
		}
		p = __lookup_perm_cap(obj, next, gs);
	} else if(next->magic == SC_DLG_MAGIC) {
		struct scdlg *nd = (void *)(dlg + 1);
		if(nd->delegatee != dlg->delegator) {
			printk("broken chain\n");
			return 0;
		}
		p = __lookup_perm_dlg(obj, nd, gs);
	} else {
		printk("invalid magic in chain\n");
	}
	if(!(p & SCP_CD)) {
		printk("chain is not CD\n");
		return 0;
	}
	if(dlg->flags & SCF_GATE) {
		__limit_gates(gs, &dlg->gates);
	}
	return p & dlg->mask;
}

static uint32_t __lookup_perm_bucket(struct object *obj, struct scbucket *b, struct scgates *gs)
{
	uintptr_t off = (uintptr_t)b->data;
	off -= OBJ_NULLPAGE_SIZE;

	struct sccap cap;
	obj_read_data(obj, off, sizeof(cap), &cap);
	if(cap.magic == SC_CAP_MAGIC) {
		return __lookup_perm_cap(obj, &cap, gs);
	} else if(cap.magic == SC_DLG_MAGIC) {
		return __lookup_perm_dlg(obj, (void *)&cap, gs);
	} else {
		printk("error - invalid perm object magic number\n");
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
	struct secctx ctx;
	obj_read_data(obj, 0, sizeof(ctx), &ctx);

	uint32_t perms = 0;
	bool gatesok = false;
	size_t slot = target % ctx.nbuckets;
	do {
		struct scbucket b;
		obj_read_data(obj, sizeof(ctx) + sizeof(b) * slot, sizeof(b), &b);

		if(b.target == target) {
			printk("    - lookup_perms: found!\n");
			struct scgates gs = { 0 };
			perms |= __lookup_perm_bucket(obj, &b, &gs) & b.pmask;
			if(ipoff) {
				__limit_gates(&gs, &b.gatemask);
				if(__in_gate(&gs, ipoff))
					gatesok = true;
			}
		}

		slot = b.chain;
	} while(slot != 0);
	uint32_t dfl = 0;
	struct metainfo mi;
	struct object *t = obj_lookup(target);
	if(t) {
		obj_read_data(t, OBJ_MAXSIZE - (OBJ_METAPAGE_SIZE + OBJ_NULLPAGE_SIZE), sizeof(mi), &mi);
		dfl = mi.p_flags & (MIP_DFL_READ | MIP_DFL_WRITE | MIP_DFL_EXEC | MIP_DFL_USE);
		obj_put(t);
	}
	if(p)
		*p = perms | dfl;
	if(ingate)
		*ingate = gatesok;

	return 0;
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
	printk("[%ld] fault_resolve - loaddr=%lx, vaddr=%lx, ip=%lx, target=" IDFMT ", flags=%s\n",
	  t->id,
	  loaddr,
	  vaddr,
	  ip,
	  IDPR(target),
	  fls);
	spinlock_acquire_save(&t->sc_lock);
	if(t->active_sc && t->active_sc->repr == 0) {
		spinlock_release_restore(&t->sc_lock);
		*perms = OBJSPACE_EXEC_U | OBJSPACE_WRITE | OBJSPACE_READ;
		return 0;
	}

	struct object *obj;
	uint32_t p;
	if(t->active_sc) {
		obj = obj_lookup(t->active_sc->repr);
		if(!obj) {
			panic("no repr " IDFMT, IDPR(t->active_sc->repr));
		}
		printk("  - trying active context (" IDFMT ")\n", IDPR(obj->id));
		__lookup_perms(obj, target, 0, &p, NULL);
		printk("    - p = %x (%x): %s\n", p, needed, (p & needed) == needed ? "ok" : "FAIL");
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
	}

	for(int i = 0; i < MAX_SC; i++) {
		if(!t->attached_scs[i] || t->attached_scs[i] == t->active_sc)
			continue;
		obj = obj_lookup(t->attached_scs[i]->repr);
		if(!obj) {
			panic("no repr " IDFMT, IDPR(t->attached_scs[i]->repr));
		}
		printk("  - trying " IDFMT "\n", IDPR(obj->id));
		size_t ipoff = 0;
		bool gok;
		if(flags & OBJSPACE_FAULT_EXEC) {
			assert(ip == vaddr);
			ipoff = ip % OBJ_MAXSIZE;
		}
		/* also check ip */
		objid_t id;
		if(!vm_vaddr_lookup((void *)ip, &id, NULL)) {
			struct fault_object_info info = {
				.ip = ip,
				.addr = ip,
				.objid = target,
				.flags = FAULT_OBJECT_NOMAP,
			};
			thread_raise_fault(t, FAULT_OBJECT, &info, sizeof(info));

			obj_put(obj);
			spinlock_release_restore(&t->sc_lock);
			return -1;
		}
		struct object *eo = obj_lookup(id);
		if(!eo) {
			struct fault_object_info info = {
				.ip = ip,
				.addr = ip,
				.objid = id,
				.flags = FAULT_OBJECT_EXIST,
			};
			thread_raise_fault(t, FAULT_OBJECT, &info, sizeof(info));

			obj_put(obj);
			spinlock_release_restore(&t->sc_lock);
			return -1;
		}
		uint32_t ep;
		__lookup_perms(obj, eo->id, 0, &ep, NULL);
		printk("    - ep: %s\n", (ep & SCP_EXEC) ? "ok" : "FAIL");
		obj_put(eo);
		if(!(ep & SCP_EXEC)) {
			obj_put(obj);
			continue;
		}

		__lookup_perms(obj, target, ipoff, &p, &gok);
		printk(
		  "    - p = %x (%x): %s\n", p, needed, (p & needed) == needed ? "ok -- SWITCH" : "FAIL");
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
			if(t == current_thread)
				secctx_switch(i);
			return 0;
		}
		obj_put(obj);
	}
	spinlock_release_restore(&t->sc_lock);
	*perms = 0; // OBJSPACE_EXEC_U | OBJSPACE_WRITE | OBJSPACE_READ;
	struct fault_sctx_info info = {
		.ip = ip,
		.addr = ip,
		.target = target,
		.pneed = needed,
	};
	thread_raise_fault(t, FAULT_OBJECT, &info, sizeof(info));

	return 0;
}

bool secctx_thread_attach(struct sctx *s, struct thread *t)
{
	printk("thread %ld attach to " IDFMT "\n", t->id, IDPR(s->repr));
	spinlock_acquire_save(&t->sc_lock);
	if(t->active_sc->repr == 0) {
		/* bootstrap context. Get rid of it, we're a real security context now! */
		assert(t->attached_scs[0] == t->active_sc);
		t->attached_scs[0] = NULL;
		/* once for attached[0] */
		krc_put_call(t->active_sc, refs, __secctx_krc_put);
		/* and again for the active_sc */
		krc_put_call(t->active_sc, refs, __secctx_krc_put);
		t->active_sc = NULL;
	}
	bool ok = false, found = false;
	size_t i;

	for(i = 0; i < MAX_SC; i++) {
		if(t->attached_scs[i] == s) {
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
				ok = true;
				break;
			}
		}
		if(ok && t->active_sc == NULL) {
			secctx_switch(i);
		}
	}
	spinlock_release_restore(&t->sc_lock);
	return ok;
}

static bool __secctx_thread_detach(struct sctx *s, struct thread *thr)
{
	printk("thread %ld detach from " IDFMT "\n", thr->id, IDPR(s->repr));
	bool ok = false;
	ssize_t na = -1;
	for(size_t i = 0; i < MAX_SC; i++) {
		if(thr->attached_scs[i] == s) {
			krc_put_call(s, refs, __secctx_krc_put);
			thr->attached_scs[i] = NULL;
			thr->attached_scs_attrs[i] = 0;
			ok = true;
		} else if(na == -1) {
			na = i;
		}
	}
	/* TODO: maybe we could leave this? */
	if(thr->active_sc == s) {
		// thr->active_sc = NULL;
		secctx_switch(na);
	}
	return ok;
}

bool secctx_thread_detach(struct sctx *s, struct thread *thr)
{
	bool ok;
	spinlock_acquire_save(&thr->sc_lock);
	ok = __secctx_thread_detach(s, thr);
	spinlock_release_restore(&thr->sc_lock);
	return ok;
}

void secctx_become_detach(struct thread *thr)
{
	spinlock_acquire_save(&thr->sc_lock);
	for(size_t i = 0; i < MAX_SC; i++) {
		if(thr->attached_scs[i] && (thr->attached_scs_attrs[i] & TWZ_DETACH_ONBECOME)) {
			__secctx_thread_detach(thr->attached_scs[i], thr);
		}
	}
	spinlock_release_restore(&thr->sc_lock);
}

bool secctx_detach_all(struct thread *thr, int flags)
{
	bool ok = true;
	spinlock_acquire_save(&thr->sc_lock);
	for(size_t i = 0; i < MAX_SC; i++) {
		if(flags) {
			thr->attached_scs_attrs[i] = flags;
		} else {
			bool r = secctx_thread_detach(thr->attached_scs[i], thr);
			ok = ok && r;
		}
	}
	spinlock_release_restore(&thr->sc_lock);
	return ok;
}

static bool __secctx_detach(struct object *parent, struct object *child, int flags)
{
	if(parent->kso_type != KSO_THREAD || child->kso_type != KSO_SECCTX)
		return false;
	/* TODO: actually get the thread object */
	struct thread *thr = current_thread;
	struct sctx *s = child->sctx.sc;

	if(flags == 0) {
		/* detach now */
		return secctx_thread_detach(s, thr);
	}
	bool ok = false;
	spinlock_acquire_save(&thr->sc_lock);
	for(size_t i = 0; i < MAX_SC; i++) {
		if(thr->attached_scs[i] == s) {
			thr->attached_scs_attrs[i] = flags;
			ok = true;
			break;
		}
	}
	spinlock_release_restore(&thr->sc_lock);
	return ok;
}

static bool __secctx_attach(struct object *parent, struct object *child, int flags)
{
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
	.ctor = __secctx_ctor,
};

__initializer static void __init_kso_secctx(void)
{
	kso_register(KSO_SECCTX, &__ksoc_sctx);
}
