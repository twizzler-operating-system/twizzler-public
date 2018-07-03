#include <secctx.h>
#include <slab.h>
#include <thread.h>

static void _sc_ctor(void *_x __unused, void *ptr)
{
	struct secctx *sc = ptr;
	arch_secctx_init(sc);
}

static void _sc_dtor(void *_x __unused, void *ptr)
{
	struct secctx *sc = ptr;
	arch_secctx_destroy(sc);
}

DECLARE_SLABCACHE(sc_sc, sizeof(struct secctx), _sc_ctor, _sc_dtor, NULL);

struct secctx *secctx_alloc(objid_t repr)
{
	struct secctx *s = slabcache_alloc(&sc_sc);
	krc_init(&s->refs);
	s->repr = repr;
	return s;
}

void secctx_free(struct secctx *s)
{
	return slabcache_free(s);
}

bool secctx_thread_attach(struct secctx *s, struct thread *t)
{
	spinlock_acquire_save(&t->sc_lock);
	bool ok = false;
	for(int i=0;i<MAX_SC;i++) {
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

