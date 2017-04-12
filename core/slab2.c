struct cache;
struct slab {
	struct slab *next, *prev;
	uint64_t alloc;
	struct cache *cache;
	_Alignas(sizeof(void *)) char data[];
};

struct cache {
	struct slab empty, partial, full;
	void (*ctor)(void *, void *);
	void (*dtor)(void *, void *);
	size_t sz;
	void *ptr;
};

/* TODO: optimization: we could try to fit several slabs in a
 * single page if the object size is small enough (sz*64 < PAGE_SIZE/2)
 */

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

void print_binary(uint64_t x)
{
	printf("%llx:0b", x);
	for(int i=0;i<64;i++) {
		printf("%c", (x & (1ull << (63-i))) ? '1' : '0');
	}
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

#include <errno.h>
#include <string.h>
struct slab *new_slab(struct cache *c)
{
	struct slab *s = mmap(NULL, slab_size(c->sz), PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(((uintptr_t)s & 0xFFF) == 0);
	assert(s != MAP_FAILED);
	s->alloc = 0;
	s->cache = c;

	for(int i=0;i<obj_per_slab(c->sz);i++) {
		s->alloc |= (1ull << i);
		if(c->ctor) {
			char *obj = s->data + i * c->sz;
			c->ctor(c->ptr, obj);
		}
	}
	return s;
}

void *alloc_slab(struct slab *s, int new)
{
	assert(s->alloc);
	int slot = first_set_bit(s->alloc);

	s->alloc &= ~(1ull << slot);

	char *ret = s->data + s->cache->sz * slot;

	if(num_set(s->alloc) == 0) {
		del_from_list(s);
		add_to_list(&s->cache->full, s);
	} else if(num_set(s->alloc) == obj_per_slab(s->cache->sz) - 1) {
		if(!new) {
			del_from_list(s);
		}
		add_to_list(&s->cache->partial, s);
	}

	return ret;
}

long long mean = 0;
int count=0;

void *cache_alloc(struct cache *c)
{
	long long st = rdtsc();
	struct slab *s;
	int new = 0;
	if(!is_empty(c->partial)) {
		s = c->partial.next;
	} else if(!is_empty(c->empty)) {
		s = c->empty.next;
	} else {
		s = new_slab(c);
		new = 1;
	}

	void *ret = alloc_slab(s, new);
	long long en = rdtsc();


	long long d = en - st;
	mean = (mean * count + d) / (count + 1);
	count++;
}

void cache_free(void *obj)
{
	struct slab *s = (struct slab *)((uintptr_t)obj & (~(PAGE_SIZE - 1)));
	int slot = ((char *)obj - s->data) / s->cache->sz;

	s->alloc |= (1ull << slot);
	if(num_set(s->alloc) == obj_per_slab(s->cache->sz)) {
		del_from_list(s);
		add_to_list(&s->cache->empty, s);
	} else if(num_set(s->alloc) == 1) {
		del_from_list(s);
		add_to_list(&s->cache->partial, s);
	}
}

void destroy_slab(struct slab *s)
{
	assert(num_set(s->alloc) == obj_per_slab(s->cache->sz));
	for(int i=0;i<obj_per_slab(s->cache->sz);i++) {
		if(s->cache->dtor) {
			char *obj = s->data + i * s->cache->sz;
			s->cache->dtor(s->cache->ptr, obj);
		}
	}
}

void cache_reap(struct cache *c)
{
	for(struct slab *s = c->empty.next;s != &c->empty; s=s->next) {
		printf("Reaping slab %p\n", s);
		destroy_slab(s);
	}
}

static void init_list(struct slab *s)
{
	s->next = s;
	s->prev = s;
}

void cache_init(struct cache *c, size_t sz,
		void (*ctor)(void *, void *), void (*dtor)(void *, void *), void *ptr)
{
	c->ptr = ptr;
	c->sz = sz;
	c->ctor = ctor;
	c->dtor = dtor;

	init_list(&c->empty);
	init_list(&c->full);
	init_list(&c->partial);
	printf("Creating cache with obj size %d\n", c->sz);
}
