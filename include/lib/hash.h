#ifndef __LIB_HASH
#define __LIB_HASH

#include <stdint.h>
#include <spinlock.h>
#include <lib/linkedlist.h>

#define HASH_LOCKLESS 1
struct hashelem {
	void *ptr;
	const void *key;
	size_t keylen;
	struct linkedentry entry;
};

struct hash {
	struct linkedlist *table;
	_Atomic size_t length, count;
	int flags;
	struct spinlock lock;
};

struct hashiter {
	struct hash *hash;
	unsigned int bucket;
	struct linkedentry *entry, *next;
};

void __hash_unlock(struct hash *h);
void __hash_lock(struct hash *h);

static inline void *hash_iter_get(struct hashiter *iter) { return ((struct hashelem *)iter->entry->obj)->ptr; }
static inline bool hash_iter_done(struct hashiter *iter)
{
	return iter->bucket >= iter->hash->length;
}

void hash_iter_init(struct hashiter *iter, struct hash *h);
void hash_iter_next(struct hashiter *iter);
static inline size_t hash_count(struct hash *h) { return h->count; }
static inline size_t hash_length(struct hash *h) { return h->length; }

void hash_create(struct hash *h, int flags, size_t length);
void hash_destroy(struct hash *h);
int hash_insert(struct hash *h, const void *key, size_t keylen, struct hashelem *elem, void *data);
int hash_delete(struct hash *h, const void *key, size_t keylen);
void *hash_lookup(struct hash *h, const void *key, size_t keylen);

#endif
