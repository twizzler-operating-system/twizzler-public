#pragma once

#include <spinlock.h>

struct ihelem {
	struct ihelem *next, *prev;
};

struct ihtable {
	int bits;
	struct ihelem *table[];
};

#define ihtable_size(bits) \
	(sizeof(struct ihtable) + ((1ul << (bits)) + 1) * sizeof(struct ihelem))

#define DECLARE_IHTABLE(name, nbits) \
	struct ihtable name = { \
		.table = { [0 ... (1ul << nbits)] = 0 }, \
		.bits = nbits, \
	}

static inline void ihtable_init(struct ihtable *t, int bits)
{
	for(size_t i=0;i<(1ul << bits);i++) {
		t->table[i] = NULL;
	}
	t->bits = bits;
}

#define ihtable_iter_start(t) 0
#define ihtable_iter_next(i) ({ (i) + 1; })
#define ihtable_iter_end(t) ({ 1ul << (t)->bits; })

#define ihtable_bucket_iter_start(t, i) ({ (t)->table[(i)]; })
#define ihtable_bucket_iter_next(b) ({ b->next; })
#define ihtable_bucket_iter_end(t) ({ NULL; })

__attribute__((used))
static void _iht_ctor(void *sz, void *obj)
{
	struct ihtable *iht = obj;
	ihtable_init(iht, (long)sz);
}

#define GOLDEN_RATIO_64 0x61C8864680B583EBull

static inline uint64_t hash64(uint64_t val)
{
	return val * GOLDEN_RATIO_64;
}

static inline size_t hash64_sz(uint64_t key, int bits)
{
	return key * GOLDEN_RATIO_64 >> (sizeof(size_t)*8 - bits);
}

static inline size_t hash128_sz(uint128_t key, int bits)
{
	return hash64((uint64_t)key ^ hash64(key >> 64)) >> (sizeof(size_t)*8 - bits);
}

#define ihtable_insert(t, e, key) \
	__ihtable_insert((t), \
		sizeof(key) > 8 ? hash128_sz((key), (t)->bits) : hash64_sz((key), (t)->bits), \
		(e))

static inline void __ihtable_insert(struct ihtable *t, int bucket, struct ihelem *e)
{
	e->next = t->table[bucket];
	if(t->table[bucket]) t->table[bucket]->prev = e;
	t->table[bucket] = e;
	e->prev = NULL;
}

#define ihtable_remove(t, e, key) \
	__ihtable_remove((t), \
		sizeof(key) > 8 ? hash128_sz((key), (t)->bits) : hash64_sz((key), (t)->bits), \
		(e))

static inline void __ihtable_remove(struct ihtable *t, int bucket, struct ihelem *e)
{
	if(e->prev == NULL) {
		t->table[bucket] = e->next;
	} else {
		e->prev->next = e->next;
	}
	if(e->next) {
		e->next->prev = e->prev;
	}
}

#define ihtable_find(t,key,type,memb,keymemb) ({\
		type *ret = NULL; \
		int bucket = sizeof(key) > 8 ? hash128_sz((key), (t)->bits) \
		                             : hash64_sz((key), (t)->bits); \
		for(struct ihelem *e = (t)->table[bucket];e;e=e->next) { \
			type *__obj = container_of(e, type, memb); \
			if(__obj->keymemb == (key)) { \
				ret = __obj; \
				break; \
			} \
		}; \
		ret;})

