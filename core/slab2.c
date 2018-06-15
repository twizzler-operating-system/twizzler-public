#include <slab.h>
/* TODO (minor): optimization: we could try to fit several slabs in a
 * single page if the object size is small enough (sz*64 < PAGE_SIZE/2)
 */

/* TODO (major): remove this hard coding */
#define PAGE_SIZE 0x1000

static inline size_t slab_size(size_t sz)
{
	size_t tmp = sz * 64;
	if(tmp < PAGE_SIZE) tmp = PAGE_SIZE;
	if(tmp > PAGE_SIZE * 2) tmp = PAGE_SIZE * 2;
	return tmp;
}

#define slab_size(sz) (sizeof(struct slab) + 64 * sz)

#define is_empty(x) ((x).next == &(x))

#define first_set_bit(x) __builtin_ctzll(x)
#define num_set(x) __builtin_popcountll(x)
#define obj_per_slab(sz) ((slab_size(sz) - sizeof(struct slab)) / (sz))

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

#include <debug.h>
#include <memory.h>
static struct slab *new_slab(struct slabcache *c)
{
	struct slab *s = (void *)mm_virtual_alloc(slab_size(c->sz), PM_TYPE_DRAM, true);
	s->alloc = 0;
	s->slabcache = c;

	for(unsigned int i=0;i<obj_per_slab(c->sz);i++) {
		s->alloc |= (1ull << i);
		if(c->ctor) {
			char *obj = s->data + i * c->sz;
			c->ctor(c->ptr, obj);
		}
	}
	return s;
}

static void *alloc_slab(struct slab *s, int new)
{
	assert(s->alloc);
	int slot = first_set_bit(s->alloc);

	s->alloc &= ~(1ull << slot);

	char *ret = s->data + s->slabcache->sz * slot;

	if(num_set(s->alloc) == 0) {
		del_from_list(s);
		add_to_list(&s->slabcache->full, s);
	} else if(num_set(s->alloc) == obj_per_slab(s->slabcache->sz) - 1) {
		if(!new) {
			del_from_list(s);
		}
		add_to_list(&s->slabcache->partial, s);
	}

	return ret;
}

void *slabcache_alloc(struct slabcache *c)
{
	struct slab *s;
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
	spinlock_release(&c->lock, fl);
	return ret;
}

void slabcache_free(void *obj)
{
	struct slab *s = (struct slab *)((uintptr_t)obj & (~(PAGE_SIZE - 1)));
	int slot = ((char *)obj - s->data) / s->slabcache->sz;

	bool fl = spinlock_acquire(&s->slabcache->lock);
	s->alloc |= (1ull << slot);
	if(num_set(s->alloc) == obj_per_slab(s->slabcache->sz)) {
		del_from_list(s);
		add_to_list(&s->slabcache->empty, s);
	} else if(num_set(s->alloc) == 1) {
		del_from_list(s);
		add_to_list(&s->slabcache->partial, s);
	}
	spinlock_release(&s->slabcache->lock, fl);
}

static void destroy_slab(struct slab *s)
{
	assert(num_set(s->alloc) == obj_per_slab(s->slabcache->sz));
	for(unsigned int i=0;i<obj_per_slab(s->slabcache->sz);i++) {
		if(s->slabcache->dtor) {
			char *obj = s->data + i * s->slabcache->sz;
			s->slabcache->dtor(s->slabcache->ptr, obj);
		}
	}
}

void slabcache_reap(struct slabcache *c)
{
	for(struct slab *s = c->empty.next;s != &c->empty; s=s->next) {
		destroy_slab(s);
	}
}

static void init_list(struct slab *s)
{
	s->next = s;
	s->prev = s;
}

void slabcache_init(struct slabcache *c, size_t sz,
		void (*ctor)(void *, void *), void (*dtor)(void *, void *), void *ptr)
{
	c->ptr = ptr;
	c->sz = sz;
	c->ctor = ctor;
	c->dtor = dtor;
	c->lock = SPINLOCK_INIT;

	init_list(&c->empty);
	init_list(&c->full);
	init_list(&c->partial);
}

