#include <memory.h>
#include <slab.h>
#include <lib/inthash.h>
#include <processor.h>

struct slabcache sc_vmctx, sc_iht, sc_vmap;

struct vmap {
	uint128_t target;
	size_t slot;
	uint32_t flags;
	int status;

	struct ihelem elem;
};

static void _vmctx_ctor(void *_p, void *obj)
{
	(void)_p;
	struct vm_context *v = obj;
	v->maps = slabcache_alloc(&sc_iht);
}

static void _vmctx_dtor(void *_p, void *obj)
{
	(void)_p;
	struct vm_context *v = obj;
	slabcache_free(v->maps);
}

static void _iht_ctor(void *_p, void *obj)
{
	(void)_p;
	struct ihtable *iht = obj;
	ihtable_init(iht, 4);
}

__initializer
static void _init_vmctx(void)
{
	slabcache_init(&sc_iht, ihtable_size(4), _iht_ctor, NULL, NULL);
	slabcache_init(&sc_vmap, sizeof(struct vmap), NULL, NULL, NULL);
	slabcache_init(&sc_vmctx, sizeof(struct vm_context), _vmctx_ctor, _vmctx_dtor, NULL);
}

struct vm_context *vm_context_create(void)
{
	return slabcache_alloc(&sc_vmctx);
}

void vm_context_destroy(struct vm_context *v)
{
	slabcache_free(v);
}

int vm_context_map(struct vm_context *v, uint128_t objid, size_t slot, uint32_t flags)
{
	ihtable_lock(v->maps);
	struct vmap *m = ihtable_find(v->maps, slot, struct vmap, elem, slot);
	if(m) {
		ihtable_unlock(v->maps);
		return -1;
	}
	m = slabcache_alloc(&sc_vmap);
	m->slot = slot;
	m->target = objid;
	m->flags = flags;
	m->status = 0;
	ihtable_insert(v->maps, &m->elem, m->slot);
	ihtable_unlock(v->maps);
	return 0;
}

void vm_context_fault(uintptr_t addr, int flags)
{
	size_t slot = addr / mm_page_size(MAX_PGLEVEL);
	struct vmap *map = ihtable_find(current_thread->ctx->maps, slot, struct vmap, elem, slot);
	printk("mapping virt slot %ld -> obj " PR128FMTd "\n", map->slot, PR128(map->target));
	panic("here %lx %x: %p", addr, flags, map);
}

