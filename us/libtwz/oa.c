#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <twz/_err.h>
#include <twz/alloc.h>
#include <twz/obj.h>

#define MIN_SIZE 16
#include <twz/debug.h>

#define USE_SLAB 1

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

//#define DEBUG(...) debug_printf(__VA_ARGS__)
#define DEBUG(...)
static inline int get_size_class(size_t sz)
{
	for(unsigned int i = 0; i < NR_SZS; i++) {
		if(sz <= size_classes[i] * MIN_SIZE)
			return i;
	}
	return -1;
}

static inline size_t nr_objs(struct slab *slab)
{
	return (PAGE_SIZE - sizeof(*slab)) / (slab->sz);
}

static inline bool is_in_slab(struct slab *slab, void *obj)
{
	return ((char *)obj > (char *)slab && (char *)obj < (char *)slab + PAGE_SIZE);
}

static inline uint8_t *get_bitmap(twzobj *o, struct twzoa_header *hdr)
{
	return (void *)((char *)twz_object_base(o) + hdr->start);
}

static void __init_slab(twzobj *o, struct twzoa_header *hdr)
{
	size_t nrpages = ((hdr->end - hdr->start) - 1) / PAGE_SIZE + 1;
	for(unsigned int i = 0; i < NR_SZS; i++) {
		hdr->slb.lists[i].partial = NULL;
	}
	hdr->slb.pg_partial = NULL;
	hdr->slb.pgbm_len = nrpages;
	hdr->slb.last = 0;
	uint8_t *bm = get_bitmap(o, hdr);
	memset(bm, 0, ((nrpages - 1) / 8) + 1);
	for(unsigned int i = 0; i < ((nrpages - 1) / (PAGE_SIZE) + 1); i++) {
		bm[i / 8] |= (1 << (i % 8));
	}
	DEBUG("A\n");
}

static void *get_slab(twzobj *o, struct twzoa_header *hdr, int sc)
{
	DEBUG("--alloc sc = %d (%ld)\n", sc, size_classes[sc] * MIN_SIZE);
	struct slab *slab = hdr->slb.lists[sc].partial;
	// long long a = rdtsc();
	if(!slab) {
		/* TODO: allocate from pages */
		uint8_t *bm = get_bitmap(o, hdr);
		DEBUG("  bitmap_start: %p\n", bm);
		size_t len = PAGE_SIZE;
		size_t nrpages = (len - 1) / PAGE_SIZE + 1;
		void *start;
		/* TODO: next-fit */
		size_t i;
		for(i = hdr->slb.last; i != hdr->slb.last - 1; i = (i + 1) % hdr->slb.pgbm_len) {
			DEBUG("  SCANNING: %ld\n", i);
			if(!(bm[i / 8] & (1 << (i % 8)))) {
				/* found an empty slot... */
				size_t j;
				for(j = 0; j < nrpages; j++) {
					if((bm[(i + j) / 8] & (1 << ((i + j) % 8)))) {
						break;
					}
				}
				if(j == nrpages) {
					/* found */
					goto found;
				}
				i += j - 1;
			}
		}
		DEBUG("---> %ld\n", i + 1);
		errno = -ENOMEM;
		return NULL;
	found:
		hdr->slb.last = i + nrpages;
		//	long long b = rdtsc();
		for(size_t j = i; j < i + nrpages; j++) {
			DEBUG("  PAGE alloc %ld\n", j);
			bm[j / 8] |= (1 << (j % 8));
		}
		start = (char *)twz_object_base(o) + hdr->start + i * PAGE_SIZE;
		DEBUG("  allocated new pages starting at %p (%ld, %lx)\n", start, i, hdr->start);
		//	long long c = rdtsc();
		slab = start;
		//	long long d = rdtsc();
		slab->next = NULL;
		slab->nrobj = 0;
		slab->alloc[0] = ~0ull;
		slab->alloc[1] = ~0ull;
		slab->alloc[2] = ~0ull;
		slab->alloc[3] = ~0ull;
		slab->sz = size_classes[sc] * MIN_SIZE;
		hdr->slb.lists[sc].partial = twz_ptr_local(slab);
		//	long long e = rdtsc();
		//	debug_printf("== %ld %ld %ld %ld\n", (b - a), (c - b), (d - c), (e - d));
	} else {
		slab = twz_object_lea(o, slab);
	}
	// got_slab:

	// DEBUG("  allocating from slab %p (%d / %d)\n", slab, slab->nrobj, nr_objs(slab));
	DEBUG("  allocating from slab %p (%d) sz=%d\n", slab, slab->nrobj, slab->sz);

	int i, x;
	for(x = 0; x < 4; x++) {
		DEBUG("    trying %lx\n", slab->alloc[x]);
		if(slab->alloc[x]) {
			i = __builtin_ffsll(slab->alloc[x]) - 1;
			break;
		}
	}

	slab->alloc[x] &= ~(1ull << i);
	slab->nrobj++;
	DEBUG("  obj = %d,%d: %d (alloc -> %lx)\n", x, i, x * 64 + i, slab->alloc[x]);

	void *ret = (char *)slab + sizeof(*slab) + ((x * 64) + i) * slab->sz;
	DEBUG("  ret = %p\n", ret);
	DEBUG(":: %d\n", slab->sz);
	if(slab->nrobj == nr_objs(slab)) {
		hdr->slb.lists[sc].partial = slab->next;
	}

	return twz_ptr_local(ret);
}

static void __slab_free(twzobj *o, struct twzoa_header *hdr, void *p)
{
	void *vp = twz_object_lea(o, p);

	DEBUG("--free %p\n", p);

	struct slab *slab = (void *)((uintptr_t)vp & ~(PAGE_SIZE - 1));
	int obj = ((char *)vp - ((char *)slab + sizeof(*slab))) / slab->sz;
	int sc = get_size_class(slab->sz);
	DEBUG("  sc = %d, obj = %d\n", sc, obj);
	DEBUG("  slab calculated as %p\n", slab);

	if(slab->nrobj == nr_objs(slab)) {
		/* was full */
		slab->next = hdr->slb.lists[sc].partial;
		hdr->slb.lists[sc].partial = twz_ptr_local(slab);
	}
	slab->alloc[obj / 64] |= (1ull << (obj % 64));
	slab->nrobj--;
	if(slab->nrobj == 0) {
		struct slab **prev = &hdr->slb.lists[sc].partial;
		for(; twz_object_lea(o, *prev) != slab;) {
			prev = &(twz_object_lea(o, *prev)->next);
		}
		*prev = slab->next;

		uint8_t *bm = get_bitmap(o, hdr);
		size_t pg = ((uintptr_t)twz_ptr_local(slab) - (hdr->start + OBJ_NULLPAGE_SIZE)) / PAGE_SIZE;
		hdr->slb.last = pg;
		size_t nr = 1;
		for(size_t n = 0; n < nr; n++) {
			bm[(pg + n) / 8] &= ~(1 << ((pg + n) % 8));
			DEBUG("  free page %ld\n", pg + n);
		}
	}
}

static void *__slab_alloc(twzobj *o, struct twzoa_header *hdr, size_t size)
{
	int sc = get_size_class(size);
	if(sc == -1) {
		/* TODO: large allocations */
		abort();
	}
	return get_slab(o, hdr, sc);
}

#if 1
void slab_test()
{
	twzobj o;
	twz_object_new(&o, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);

	DEBUG("SLAB TEST\n");
	struct twzoa_header *hdr = twz_object_base(&o);
	oa_hdr_init(&o, hdr, 0x3000, OBJ_MAXSIZE - 0x8000);

	long long s = rdtsc();
	for(size_t i = 0; i < 1000000; i++) {
		void *r = oa_hdr_alloc(&o, hdr, 12);
		asm volatile("" ::"r"(r) : "memory");
	}
	long long e = rdtsc();

	debug_printf(":: %ld\n", (e - s) / (4 * 1000000));

	for(;;)
		;
}
#endif

/* based on https://github.com/matianfu/buddy/blob/master/buddy.c */

/* TODO: robust, persist */

#define POOLSIZE (1ul << MAX_ORDER)
#define BLOCKSIZE(i) (1ul << (i))

#define OFF(b, h) ((uintptr_t)b - (h)->start)
#define _BOF(b, i, h) (OFF(b, h) ^ (1 << (i)))
#define BOF(b, i, h) ((void *)(_BOF(b, i, h) + (h)->start))

static void *__buddy_alloc(twzobj *o, struct twzoa_header *hdr, size_t size)
{
	unsigned int i = 0, order;
	size++;

	while(BLOCKSIZE(i) < size)
		i++;
	order = i = (i < MIN_ORDER) ? MIN_ORDER : i;

	for(;; i++) {
		if(i > hdr->bdy.max_order)
			return NULL;
		if(hdr->bdy.flist[i])
			break;
	}

	void *block = hdr->bdy.flist[i];
	void *vblock = twz_object_lea(o, block);
	if(block == (void *)4) {
		for(int j = 0; j < MAX_ORDER + 2; j++) {
			DEBUG("       %p\n", hdr->bdy.flist[j]);
		}
	}
	DEBUG("::: %d %p %p\n", i, block, vblock);
	hdr->bdy.flist[i] = *(void **)vblock;
	DEBUG(":: %p\n", *(void **)vblock);

	while(i-- > order) {
		void *buddy = BOF(block, i, hdr);
		DEBUG("assign %d %p\n", i, buddy);
		hdr->bdy.flist[i] = buddy;
	}
	*(((uint8_t *)vblock) - 1) = (uint8_t)order;
	return block;
}

static void __buddy_free(twzobj *o, struct twzoa_header *hdr, void *block)
{
	void *vblock = twz_object_lea(o, block);
	// fetch order in previous byte
	int i = *((uint8_t *)vblock - 1);

	for(;; i++) {
		// calculate buddy
		void *buddy = BOF(block, i, hdr);
		void **p = &(hdr->bdy.flist[i]);

		// find buddy in list
		while((*p != NULL) && (*p != buddy)) {
			void **vp = twz_object_lea(o, (*p));
			p = (void **)*vp;
		}

		// not found, insert into list
		if(*p != buddy) {
			DEBUG("--> %d %p %p\n", i, hdr->bdy.flist[i], block);
			*(void **)vblock = hdr->bdy.flist[i];
			hdr->bdy.flist[i] = block;
			return;
		}
		// found, merged block starts from the lower one
		block = (block < buddy) ? block : buddy;
		// remove buddy out of list
		void **vp = twz_object_lea(o, (*p));
		DEBUG("--> :: %p %p\n", *p, vp);
		*p = (void *)*vp;
	}
}

void oa_hdr_free(twzobj *obj, struct twzoa_header *hdr, void *p)
{
	mutex_acquire(&hdr->m);
	DEBUG(":: free  = %p\n", p);
#if USE_SLAB
	__slab_free(obj, hdr, p);
#else
	__buddy_free(obj, hdr, p);
#endif
	mutex_release(&hdr->m);
}

void *oa_hdr_alloc(twzobj *obj, struct twzoa_header *hdr, size_t s)
{
	mutex_acquire(&hdr->m);
#if USE_SLAB
	void *r = __slab_alloc(obj, hdr, s);
#else
	void *r = __buddy_alloc(obj, hdr, s);
#endif
	DEBUG(":: alloc = %p\n", r);
	mutex_release(&hdr->m);
	return r;
}

int oa_hdr_init(twzobj *obj, struct twzoa_header *h, size_t start, size_t end)
{
	(void)obj;
	// start += 16;
	h->start = start;
	h->end = end;
	mutex_init(&h->m);
#if USE_SLAB
	__init_slab(obj, h);
#else
	h->bdy.max_order = 0;
	memset(h->bdy.flist, 0, sizeof(h->bdy.flist));
	while(BLOCKSIZE(h->bdy.max_order + 1) < end - start && h->bdy.max_order < MAX_ORDER) {
		h->bdy.max_order++;
	}
	if(h->bdy.max_order <= MIN_ORDER)
		return -ENOSPC;
	h->bdy.flist[h->bdy.max_order] = (void *)start;
#endif
	return 0;
}
