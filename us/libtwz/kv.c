#include <stdio.h>
#include <stdatomic.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <bstream.h>
#include <twzcore.h>
#include <twzslots.h>
#include <string.h>

#include <debug.h>
#include <errno.h>
#include <twzthread.h>

#define printf debug_printf

#define fprintf(x, ...) \
	debug_printf(__VA_ARGS__)

#include <twzkv.h>

static const size_t g_a_sizes[] =
{
  /* 0     */              5ul,
  /* 1     */              11ul, 
  /* 2     */              23ul, 
  /* 3     */              47ul, 
  /* 4     */              97ul, 
  /* 5     */              199ul, 
  /* 6     */              409ul, 
  /* 7     */              823ul, 
  /* 8     */              1741ul, 
  /* 9     */              3469ul, 
  /* 10    */              6949ul, 
  /* 11    */              14033ul, 
  /* 12    */              28411ul, 
  /* 13    */              57557ul, 
  /* 14    */              116731ul, 
  /* 15    */              236897ul,
  /* 16    */              480881ul, 
  /* 17    */              976369ul,
  /* 18    */              1982627ul, 
  /* 19    */              4026031ul,
  /* 20    */              8175383ul, 
  /* 21    */              16601593ul, 
  /* 22    */              33712729ul,
  /* 23    */              68460391ul, 
  /* 24    */              139022417ul, 
  /* 25    */              282312799ul, 
  /* 26    */              573292817ul, 
  /* 27    */              1164186217ul,
  /* 28    */              2364114217ul, 
  /* 29    */              4294967291ul,
};

#define NUMSLOTS 8

struct slot {
	struct twzkv_item key[NUMSLOTS];
	struct twzkv_item value[NUMSLOTS];
};

struct indexheader {
	uint32_t flags;
	uint32_t szidx;
	uint64_t count;
	struct slot *slots;
};

struct dataheader {
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
	return hash % g_a_sizes[ih->szidx];
}

static bool compare(struct object *index, struct twzkv_item *k1,
		struct twzkv_item *k2, bool sk1, bool sk2)
{
	void *d1 = k1->data;
	void *d2 = k2->data;
	if(sk1) d1 = twz_ptr_lea(index, d1);
	if(sk2) d2 = twz_ptr_lea(index, d2);
	return k1->length == k2->length && !memcmp(d1, d2, k1->length);
}

void twzkv_init_index(struct object *index)
{
	struct indexheader *ih = twz_ptr_base(index);
	ih->szidx = 2;
	ih->slots = (void *)twz_ptr_local(ih+1);
}

void twzkv_init_data(struct object *data)
{
	struct dataheader *dh = twz_ptr_base(data);
	dh->end = twz_ptr_local(dh+1);
}

int twzkv_create_index(struct object *index)
{
	int ret = twz_object_new(index, NULL, 0, 0, TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE);
	if(ret) return ret;
	twzkv_init_index(index);
	return 0;
}

int twzkv_create_data(struct object *data)
{
	int ret = twz_object_new(data, NULL, 0, 0, TWZ_ON_DFL_READ | TWZ_ON_DFL_WRITE);
	if(ret) return ret;
	twzkv_init_data(data);
	return 0;
}


static int _ht_lookup(struct object *index, struct indexheader *ih,
		struct twzkv_item *key, struct twzkv_item *value)
{
	size_t b = __hash(key, ih);
	struct slot *table = twz_ptr_lea(index, ih->slots);
	for(int i=0;i<NUMSLOTS;i++) {
		if(table[b].key[i].data) {
			if(compare(index, key, &table[b].key[i], false, true)) {
				if(value) {
					value->length = table[b].value[i].length;
					value->data = twz_ptr_lea(index, table[b].value[i].data);
				}
				return 0;
			}
		}
	}
	return -1;
}

static int _ht_insert(struct object *index, ssize_t, struct indexheader *ih,
		struct twzkv_item *key, struct twzkv_item *value);
static void _ht_rehash(struct object *index, struct indexheader *ih, ssize_t idx_to_data)
{
	/* TODO (perf): this is slow */
	struct slot *table = twz_ptr_lea(index, ih->slots);
	struct slot *backup = calloc(g_a_sizes[ih->szidx], sizeof(struct slot));
	memcpy(backup, table, g_a_sizes[ih->szidx] * sizeof(struct slot));

	uint32_t oldsi = ih->szidx++;
	memset(table, 0, g_a_sizes[oldsi] * sizeof(struct slot));

	uint64_t count = ih->count;
	for(size_t b=0;b<g_a_sizes[oldsi];b++) {
		for(int i=0;i<NUMSLOTS;i++) {
			_ht_insert(index, idx_to_data, ih, &backup[b].key[i], &backup[b].value[i]);
		}
	}
	ih->count = count;
}

static int _ht_insert(struct object *index, ssize_t idx_to_data, struct indexheader *ih,
		struct twzkv_item *key, struct twzkv_item *value)
{
	size_t b = __hash(key, ih);
	struct slot *table = twz_ptr_lea(index, ih->slots);
	for(int i=0;i<NUMSLOTS;i++) {
		if(table[b].key[i].data == NULL) {
			table[b].key[i].length = key->length;
			table[b].value[i].length = value->length;
			if(idx_to_data > 0) {
				table[b].key[i].data = twz_ptr_rebase(idx_to_data, key->data);
				table[b].value[i].data = twz_ptr_rebase(idx_to_data, value->data);
			} else {
				table[b].key[i].data = twz_ptr_canon(index, key->data, FE_READ);
				table[b].value[i].data = twz_ptr_canon(index, value->data, FE_READ);
			}

			ih->count++;
			return 0;
		}
	}
	_ht_rehash(index, ih, idx_to_data);
	return _ht_insert(index, idx_to_data, ih, key, value);
}

static void copyin(struct object *dataobj, struct twzkv_item *item, struct twzkv_item *ret)
{
	struct dataheader *dh = twz_ptr_base(dataobj);
	void *data = dh->end;
	dh->end = (void *)((uintptr_t)dh->end + item->length);
	void *__data = twz_ptr_lea(dataobj, data);
	memcpy(__data, item->data, item->length);
	ret->data = twz_make_canon_ptr(VIRT_TO_SLOT(dataobj->base), (uintptr_t)data);
	ret->length = item->length;
}

int twzkv_put(struct object *index, struct object *data,
		struct twzkv_item *key, struct twzkv_item *value)
{
	if(twzkv_get(index, key, NULL) == 0) return -1;
	
	struct indexheader *ih = twz_ptr_base(index);
	struct twzkv_item ik, iv;
	copyin(data, key, &ik);
	copyin(data, value, &iv);

	return _ht_insert(index, 0, ih, &ik, &iv);
}

int twzkv_get(struct object *index, struct twzkv_item *key, struct twzkv_item *value)
{
	struct indexheader *ih = twz_ptr_base(index);
	return _ht_lookup(index, ih, key, value);
}

