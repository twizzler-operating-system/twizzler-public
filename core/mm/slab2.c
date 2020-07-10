#include <debug.h>
#include <memory.h>
#include <slab.h>
/* TODO (minor): optimization: we could try to fit several slabs in a
 * single page if the object size is small enough (sz*64 < PAGE_SIZE/2)
 */

static DECLARE_LIST(all_slabs);

static inline size_t __slab_size(size_t sz, size_t nr_obj)
{
	size_t x = __round_up_pow2((sizeof(struct slab) + nr_obj * sz));
	x = align_up(x, mm_page_size(0));
	return x == 0 ? mm_page_size(0) : x;
}

static inline size_t slab_size(struct slabcache *sc, size_t sz)
{
	if(sc->__cached_nr_obj) {
		return __slab_size(sz, sc->__cached_nr_obj);
	}
	size_t frag[129];
	for(int i = 0; i < 65; i++) {
		if(i < 1) {
			frag[i] = ~0;
			continue;
		}
		size_t x = __slab_size(sz, i);
		if(x > mm_page_size(0) * 2) {
			frag[i] = ~0;
		} else {
			frag[i] = x - (sizeof(struct slab) + i * sz);
		}
	}

	int best = 1;
	for(int i = 0; i < 65; i++) {
		if(frag[i] < frag[best])
			best = i;
	}

	sc->__cached_nr_obj = best;
	return __slab_size(sz, sc->__cached_nr_obj);
}

#define is_empty(x) ((x).next == &(x))

#define first_set_bit(x) __builtin_ctzll(x)
#define num_set(x) __builtin_popcountll(x)
static inline size_t obj_per_slab(struct slabcache *sc, size_t sz)
{
	size_t n = (slab_size(sc, sz) - sizeof(struct slab)) / sz;
	if(n > 64)
		n = 64;
	assert(n > 0);
	return n;
}

static inline void add_to_list(struct slab *list, struct slab *s)
{
	s->next = list->next;
	s->prev = list;
	s->next->prev = s;
	s->prev->next = s;
}

static inline void del_from_list(struct slab *s)
{
	s->next->prev = s->prev;
	s->prev->next = s->next;
}

static struct slab *new_slab(struct slabcache *c)
{
	struct slab *s = (void *)mm_memory_alloc(slab_size(c, c->sz), PM_TYPE_DRAM, true);
	s->alloc = 0;
	s->slabcache = c;
	s->canary = SLAB_CANARY;
	assert(c->canary == SLAB_CANARY);

	for(unsigned int i = 0; i < obj_per_slab(c, c->sz); i++) {
		s->alloc |= ((unsigned __int128)1ull << i);
		char *obj = s->data + i * c->sz;
		if(c->ctor) {
			c->ctor(c->ptr, obj);
		}
	}
	c->stats.total_slabs++;
	return s;
}

static void *alloc_slab(struct slab *s, int new)
{
	assert(s->alloc);
	assert(s->canary == SLAB_CANARY);
	int slot = first_set_bit(s->alloc);

	s->alloc &= ~((unsigned __int128)1ul << slot);

	char *ret = s->data + s->slabcache->sz * slot;

	if(num_set(s->alloc) == 0) {
		del_from_list(s);
		add_to_list(&s->slabcache->full, s);
		s->slabcache->stats.partial--;
		s->slabcache->stats.full++;
	} else if(num_set(s->alloc) == obj_per_slab(s->slabcache, s->slabcache->sz) - 1) {
		if(!new) {
			del_from_list(s);
			s->slabcache->stats.empty--;
		}
		add_to_list(&s->slabcache->partial, s);
		s->slabcache->stats.partial++;
	}

	size_t raw_sz = s->slabcache->sz - sizeof(struct slabmarker);
	struct slabmarker *mk = (void *)((char *)ret + raw_sz);
	mk->marker_magic = SLAB_MARKER_MAGIC;
	mk->slot = slot;

	return ret;
}

static void __slab_second_init(struct slabcache *c)
{
	if(!atomic_exchange(&c->__init, true)) {
		list_insert(&all_slabs, &c->entry);
	}
}

void *slabcache_alloc(struct slabcache *c)
{
	struct slab *s;
	assert(c->canary == SLAB_CANARY);
	__slab_second_init(c);
	int new = 0;
	bool fl = spinlock_acquire(&c->lock);
	if(!is_empty(c->partial)) {
		s = c->partial.next;
	} else if(!is_empty(c->empty)) {
		s = c->empty.next;
	} else {
		spinlock_release(&c->lock, fl);
		s = new_slab(c);
		fl = spinlock_acquire(&c->lock);
		new = 1;
	}

	void *ret = alloc_slab(s, new);
	assert(s->slabcache);
	spinlock_release(&c->lock, fl);
	c->stats.current_alloced++;
	c->stats.total_alloced++;
	return ret;
}

void slabcache_free(struct slabcache *sc, void *obj)
{
	size_t raw_sz = sc->sz - sizeof(struct slabmarker);
	struct slabmarker *mk = (void *)((char *)obj + raw_sz);
	assert(mk->marker_magic == SLAB_MARKER_MAGIC);

	struct slab *s = (struct slab *)((char *)obj - (sc->sz * mk->slot + sizeof(struct slab)));
	mk->marker_magic = 0;

	if(s->canary != SLAB_CANARY) {
		panic("SC FREE CANARY MISMATCH: %lx: %p -> %p\n", s->canary, obj, s);
	}
	assert(s->canary == SLAB_CANARY);
	int slot = ((char *)obj - s->data) / s->slabcache->sz;
	assert(s->slabcache->canary == SLAB_CANARY);

	bool fl = spinlock_acquire(&s->slabcache->lock);
	s->alloc |= ((unsigned __int128)1ull << slot);
	if(num_set(s->alloc) == obj_per_slab(s->slabcache, s->slabcache->sz)) {
		del_from_list(s);
		add_to_list(&s->slabcache->empty, s);
		sc->stats.partial--;
		sc->stats.empty++;
	} else if(num_set(s->alloc) == 1) {
		del_from_list(s);
		add_to_list(&s->slabcache->partial, s);
		sc->stats.partial++;
		sc->stats.full--;
	}
	spinlock_release(&s->slabcache->lock, fl);
	sc->stats.current_alloced--;
	sc->stats.total_freed++;
}

static void destroy_slab(struct slab *s)
{
	panic("NI");
	assert(num_set(s->alloc) == obj_per_slab(s->slabcache, s->slabcache->sz));
	for(unsigned int i = 0; i < obj_per_slab(s->slabcache, s->slabcache->sz); i++) {
		if(s->slabcache->dtor) {
			char *obj = s->data + i * s->slabcache->sz;
			s->slabcache->dtor(s->slabcache->ptr, obj);
		}
	}
}

void slabcache_reap(struct slabcache *c)
{
	for(struct slab *s = c->empty.next; s != &c->empty; s = s->next) {
		destroy_slab(s);
	}
}

static void init_list(struct slab *s)
{
	s->next = s;
	s->prev = s;
}

void slabcache_print_stats(struct slabcache *sc)
{
	printk("slabcache %s: size=%lx, slabsz=%lx, nrobj=%d\n",
	  sc->name,
	  sc->sz,
	  slab_size(sc, sc->sz),
	  sc->__cached_nr_obj);
	printk("  total_slabs: %ld (%ld empty, %ld partial, %ld full)\n",
	  sc->stats.total_slabs,
	  sc->stats.empty,
	  sc->stats.partial,
	  sc->stats.full);
	printk(
	  "  total_alloced: %ld, total_freed: %ld\n", sc->stats.total_alloced, sc->stats.total_freed);
	printk("  current_alloced: %ld\n", sc->stats.current_alloced);
}

#include <lib/iter.h>
void slabcache_all_print_stats(void)
{
	foreach(e, list, &all_slabs) {
		struct slabcache *sc = list_entry(e, struct slabcache, entry);
		slabcache_print_stats(sc);
	}
}

void slabcache_init(struct slabcache *c,
  const char *name,
  size_t sz,
  void (*ctor)(void *, void *),
  void (*dtor)(void *, void *),
  void *ptr)
{
	c->ptr = ptr;
	sz += sizeof(struct slabmarker);
	if(sz < 16)
		c->sz = align_up(sz, 8);
	else
		c->sz = align_up(sz, 16);

	c->ctor = ctor;
	c->dtor = dtor;
	c->lock = SPINLOCK_INIT;
	c->canary = SLAB_CANARY;

	init_list(&c->empty);
	init_list(&c->full);
	init_list(&c->partial);

	memset(&c->stats, 0, sizeof(c->stats));

	c->name = name;
}
