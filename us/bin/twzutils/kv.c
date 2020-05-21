#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <twz/debug.h>
#include <twz/thread.h>

#define DATAOBJ_SLOT 4
#define INDEXOBJ_SLOT 3

#include "kv.h"

#define mutex_acquire(...)
#define mutex_release(...)

static inline unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	//__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

static twzobj *dataobj;
static twzobj *indexobj;

static const size_t g_a_sizes[] = {
	/* 0     */ 5ul,
	/* 1     */ 11ul,
	/* 2     */ 23ul,
	/* 3     */ 47ul,
	/* 4     */ 97ul,
	/* 5     */ 199ul,
	/* 6     */ 409ul,
	/* 7     */ 823ul,
	/* 8     */ 1741ul,
	/* 9     */ 3469ul,
	/* 10    */ 6949ul,
	/* 11    */ 14033ul,
	/* 12    */ 28411ul,
	/* 13    */ 57557ul,
	/* 14    */ 116731ul,
	/* 15    */ 236897ul,
	/* 16    */ 480881ul,
	/* 17    */ 976369ul,
	/* 18    */ 1982627ul,
	/* 19    */ 4026031ul,
	/* 20    */ 8175383ul,
	/* 21    */ 16601593ul,
	/* 22    */ 33712729ul,
	/* 23    */ 68460391ul,
	/* 24    */ 139022417ul,
	/* 25    */ 282312799ul,
	/* 26    */ 573292817ul,
	/* 27    */ 1164186217ul,
	/* 28    */ 2364114217ul,
	/* 29    */ 4294967291ul,
};

#define NUMSLOTS 8

#define SWIZ 1
#define MULT 1
#define WARMUP 1
#define BIG 1

#include <twz/mutex.h>

struct slot {
	struct twzkv_item key[NUMSLOTS];
	struct twzkv_item value[NUMSLOTS];
	// struct mutex lock;
};

struct indexheader {
	uint32_t flags;
	uint32_t szidx;
	uint64_t count;
	struct slot *slots;
};

struct dataheader {
	struct mutex lock;
	void *end;
};

static size_t __hash(struct twzkv_item *key, struct indexheader *ih)
{
	uint64_t hash = 14695981039346656037ull;
	size_t len = key->length;
	char *str = key->data;
	while(len--) {
		hash *= 1099511628211ull;
		hash ^= *str++;
	}
	// fprintf(stderr, ":: %d -> %ld\n", *(int *)key->data, hash % g_a_sizes[ih->szidx]);
	return hash % g_a_sizes[ih->szidx];
}

/*
static int t_memcmp(void *a, void *b, size_t n)
{
    char *x = a;
    char *y = b;
    if(n == 4) {
        uint32_t *i = a;
        uint32_t *j = b;
        return *i - *j;
    }
    while(n--) {
        if(*x != *y)
            return *x - *y;
    }
    return 0;
}
*/

static bool compare(struct twzkv_item *k1, struct twzkv_item *k2, bool sk1, bool sk2)
{
	void *d1 = k1->data;
	void *d2 = k2->data;
	if(sk1)
		d1 = twz_object_lea(indexobj, d1);
#if MULT
	if(sk2)
		d2 = twz_object_lea(indexobj, d2);
#else
	if(sk2)
		d2 = twz_ptr_rebase(VADDR_TO_SLOT(dataobj->base), d2);
#endif
	return k1->length == k2->length && !memcmp(d1, d2, k1->length);
}

#define INDEX_TO_DATA 1

void init_database(twzobj *_io, twzobj *_do)
{
	dataobj = _do;
	indexobj = _io;
	int r =
	  twz_object_new(dataobj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_SYS_OC_PERSIST_);
	if(r)
		abort();
	r = twz_object_new(
	  indexobj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_SYS_OC_PERSIST_);

	if(r)
		abort();

	printf("SETTINGS:: %d %d %d %d\n", MULT, SWIZ, WARMUP, BIG);

	struct indexheader *ih = twz_object_base(indexobj);
#if BIG
	ih->szidx = 18;
#else
	ih->szidx = 2;
#endif
	ih->slots = (void *)twz_ptr_local(ih + 1);
	struct dataheader *dh = twz_object_base(dataobj);
	dh->end = twz_ptr_local(dh + 1);
	// twz_object_addfot(indexobj, twz_object_guid(dataobj), FE_READ | FE_WRITE);

#if WARMUP
	struct slot *table = twz_object_lea(indexobj, ih->slots);
	memset(table, 0, g_a_sizes[ih->szidx] * sizeof(struct slot));
	void *data = twz_object_lea(dataobj, dh->end);
	memset(data, 0, 0x100000);
#endif
}

static int _ht_lookup(struct indexheader *ih, struct twzkv_item *key, struct twzkv_item *value)
{
	size_t b = __hash(key, ih);
	struct slot *table = twz_object_lea(indexobj, ih->slots);
	mutex_acquire(&table[b].lock);
	for(int i = 0; i < NUMSLOTS; i++) {
		if(table[b].key[i].data) {
			if(compare(key, &table[b].key[i], false, !!SWIZ)) {
				if(value) {
#if SWIZ
					value->length = table[b].value[i].length;
#if MULT
					value->data = twz_object_lea(indexobj, table[b].value[i].data);
#else
					value->data =
					  twz_ptr_rebase(VADDR_TO_SLOT(dataobj->base), table[b].value[i].data);
#endif
#else
					*value = table[b].value[i];
#endif
				}
				mutex_release(&table[b].lock);
				return 0;
			}
		}
	}
	mutex_release(&table[b].lock);
	return -1;
}

// static int _ht_insert(struct indexheader *ih, struct twzkv_item *key, struct twzkv_item *value);
static void _ht_rehash(struct indexheader *ih)
{
	fprintf(stderr,
	  "REHASH %2.2f -> %ld\n",
	  (float)ih->count / (g_a_sizes[ih->szidx] * NUMSLOTS),
	  g_a_sizes[ih->szidx + 1]);
	abort();
#if 0
	struct slot *table = twz_object_lea(indexobj, ih->slots);
	struct slot *backup = calloc(g_a_sizes[ih->szidx], sizeof(struct slot));
	memcpy(backup, table, g_a_sizes[ih->szidx] * sizeof(struct slot));

	uint32_t oldsi = ih->szidx++;
	memset(table, 0, g_a_sizes[oldsi] * sizeof(struct slot));

	uint64_t count = ih->count;
	for(size_t b = 0; b < g_a_sizes[oldsi]; b++) {
		for(int i = 0; i < NUMSLOTS; i++) {
#if 0
			struct twzkv_item k = backup[b].key[i];
			struct twzkv_item v = backup[b].value[i];
			k.data = twz_ptr_rebase(VIRT_TO_SLOT(dataobj.base), k.data);
			v.data = twz_ptr_rebase(VIRT_TO_SLOT(dataobj.base), v.data);
			_ht_insert(ih, &k, &v);
#else
			_ht_insert(ih, &backup[b].key[i], &backup[b].value[i]);
#endif
		}
	}
	ih->count = count;
#endif
}

static int _ht_insert(struct indexheader *ih,
  struct twzkv_item *ok,
  struct twzkv_item *key,
  struct twzkv_item *value)
{
	size_t b = __hash(ok, ih);
	struct slot *table = twz_object_lea(indexobj, ih->slots);
	mutex_acquire(&table[b].lock);
	for(int i = 0; i < NUMSLOTS; i++) {
		if(table[b].key[i].data == NULL) {
#if SWIZ
#if MULT
			table[b].key[i].length = key->length;
			table[b].key[i].data = twz_ptr_swizzle(indexobj, key->data, FE_READ | FE_WRITE);
			table[b].value[i].length = value->length;
			table[b].value[i].data = twz_ptr_swizzle(indexobj, value->data, FE_READ | FE_WRITE);
#else
			table[b].key[i].length = key->length;
			table[b].key[i].data = twz_ptr_rebase(INDEX_TO_DATA, key->data);
			table[b].value[i].length = value->length;
			table[b].value[i].data = twz_ptr_rebase(INDEX_TO_DATA, value->data);
#endif
#else
			table[b].key[i] = *key;
			table[b].value[i] = *value;
#endif
			ih->count++;
			mutex_release(&table[b].lock);
			return 0;
		}
	}
	mutex_release(&table[b].lock);
	_ht_rehash(ih);
	return _ht_insert(ih, ok, key, value);
}

static void copyin(struct twzkv_item *item, struct twzkv_item *ret)
{
	struct dataheader *dh = twz_object_base(dataobj);
	mutex_acquire(&dh->lock);
	void *data = dh->end;
	dh->end = (void *)((uintptr_t)dh->end + item->length);
	void *__data = twz_object_lea(dataobj, data);
	memcpy(__data, item->data, item->length);
	ret->data = __data; // = twz_ptr_swizzle(indexobj, __data, FE_READ | FE_WRITE);
	ret->length = item->length;
	mutex_release(&dh->lock);
}

int twzkv_put(struct twzkv_item *key, struct twzkv_item *value)
{
	if(twzkv_get(key, NULL) == 0)
		return -1;

	struct indexheader *ih = twz_object_base(indexobj);
	struct twzkv_item ik, iv;
	copyin(key, &ik);
	copyin(value, &iv);

	int r = _ht_insert(ih, key, &ik, &iv);

	return r;
}

int twzkv_get(struct twzkv_item *key, struct twzkv_item *value)
{
	struct indexheader *ih = twz_object_base(indexobj);
	return _ht_lookup(ih, key, value);
}
