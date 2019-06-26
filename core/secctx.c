#include <object.h>
#include <secctx.h>
#include <slab.h>
#include <thread.h>
#include <twz/_sctx.h>

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

static uint32_t __lookup_perm_cap(struct object *obj, struct sccap *cap, struct scgates *gs)
{
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
	while(true) {
		struct scbucket b;
		obj_read_data(obj, sizeof(ctx) + sizeof(b) * slot, sizeof(b), &b);

		if(b.target == target) {
			struct scgates gs = { 0 };
			perms |= __lookup_perm_bucket(obj, &b, &gs) & b.pmask;
			if(ipoff) {
				__limit_gates(&gs, &b.gatemask);
				if(__in_gate(&gs, ipoff))
					gatesok = true;
			}
		}

		slot = b.chain;
	}
	if(p)
		*p = perms;
	if(ingate)
		*ingate = gatesok;

	return 0;
}

bool secctx_thread_attach(struct sctx *s, struct thread *t)
{
	spinlock_acquire_save(&t->sc_lock);
	bool ok = false;
	for(int i = 0; i < MAX_SC; i++) {
		if(t->attached_scs[i] == NULL) {
			krc_get(&s->refs);
			t->attached_scs[i] = s;
			ok = true;
			break;
		}
	}
	spinlock_release_restore(&t->sc_lock);
	return ok;
}
