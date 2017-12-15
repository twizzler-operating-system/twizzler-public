#include <stdint.h>
#include <stdbool.h>
#include <lib/linkedlist.h>
#include <spinlock.h>
#include <string.h>
#include <lib/hash.h>
#include <memory.h>
#include <system.h>
#define __lock(h) do { if(!(h->flags & HASH_LOCKLESS)) h->fl = spinlock_acquire(&h->lock); } while(0)
#define __unlock(h) do { if(!(h->flags & HASH_LOCKLESS)) spinlock_release(&h->lock, h->fl); } while(0)

void __hash_lock(struct hash *h)
{
	__lock(h);
}

void __hash_unlock(struct hash *h)
{
	__unlock(h);
}

void hash_create(struct hash *h, int flags, size_t length)
{
	h->lock = SPINLOCK_INIT;
	h->flags = flags;
	if(length * sizeof(struct linkedlist) < arch_mm_page_size(0))
		length = arch_mm_page_size(0) / sizeof(struct linkedlist);
	h->table = (void *)mm_virtual_alloc(length * sizeof(struct linkedlist), PM_TYPE_ANY, false);
	for(size_t i=0;i<length;i++) {
		linkedlist_create(&h->table[i], LINKEDLIST_LOCKLESS);
	}
	h->length = length;
}

void hash_destroy(struct hash *h)
{
	mm_virtual_dealloc((uintptr_t)h->table);
}

static size_t __hashfn(const void *key, size_t keylen, size_t table_len)
{
	size_t hash = 5381;
	const unsigned char *buf = key;
	for(unsigned int i = 0;i < keylen;i++) {
		unsigned char e = buf[i];
		hash = ((hash << 5) + hash) + e;
	}
	return hash % table_len;
}

static bool __same_keys(const void *key1, size_t key1len, const void *key2, size_t key2len)
{
	if(key1len != key2len)
		return false;
	return memcmp(key1, key2, key1len) == 0;
}

static bool __ll_check_exist(struct linkedentry *ent, void *data)
{
	struct hashelem *he = data;
	struct hashelem *this = ent->obj;
	return __same_keys(he->key, he->keylen, this->key, this->keylen);
}

int hash_insert(struct hash *h, const void *key, size_t keylen, struct hashelem *elem, void *data)
{
	__lock(h);
	size_t index = __hashfn(key, keylen, h->length);
	elem->ptr = data;
	elem->key = key;
	elem->keylen = keylen;
	struct linkedentry *ent = linkedlist_find(&h->table[index], __ll_check_exist, elem);
	if(ent) {
		__unlock(h);
		return -1;
	}
	linkedlist_insert(&h->table[index], &elem->entry, elem);
	h->count++;
	__unlock(h);
	return 0;
}

int hash_delete(struct hash *h, const void *key, size_t keylen)
{
	__lock(h);
	size_t index = __hashfn(key, keylen, h->length);
	struct hashelem tmp;
	tmp.key = key;
	tmp.keylen = keylen;
	struct linkedentry *ent = linkedlist_find(&h->table[index], __ll_check_exist, &tmp);
	if(ent) {
		linkedlist_remove(&h->table[index], ent);
		h->count--;
	}
	__unlock(h);
	return ent ? 0 : -1;
}

void *hash_lookup(struct hash *h, const void *key, size_t keylen)
{
	__lock(h);
	size_t index = __hashfn(key, keylen, h->length);
	struct hashelem tmp;
	tmp.key = key;
	tmp.keylen = keylen;
	struct linkedentry *ent = linkedlist_find(&h->table[index], __ll_check_exist, &tmp);
	void *ret = NULL;
	if(ent) {
		struct hashelem *elem = ent->obj;
		ret = elem->ptr;
	}
	__unlock(h);
	return ret;
}

void hash_iter_init(struct hashiter *iter, struct hash *h)
{
	iter->bucket = 0;
	iter->hash = h;
	
	while(h->table[iter->bucket].count == 0 && ++iter->bucket < h->length)
		;
	if(iter->bucket >= h->length)
		return;
	iter->next = linkedlist_iter_start(&h->table[iter->bucket]);

	iter->entry = iter->next;
	iter->next = linkedlist_iter_next(iter->entry);
}

void hash_iter_next(struct hashiter *iter)
{
	struct hash *h = iter->hash;
	struct linkedlist *curlist = &h->table[iter->bucket];
	if(iter->next == linkedlist_iter_end(curlist)) {
		iter->bucket++;
		while(h->table[iter->bucket].count == 0 && ++iter->bucket < h->length)
			;
		if(iter->bucket >= h->length)
			return;
		iter->next = linkedlist_iter_start(&h->table[iter->bucket]);
	}
		
	iter->entry = iter->next;
	iter->next = linkedlist_iter_next(iter->entry);
}

