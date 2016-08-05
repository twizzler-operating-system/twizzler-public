#include <lib/linkedlist.h>
#include <stdbool.h>
#include <spinlock.h>
#include <debug.h>
#include <system.h>

static void linkedlist_do_remove(struct linkedlist *list, struct linkedentry *entry)
{
	assert(entry != &list->sentry);
	assert(entry->list == list);
	assert(list->count > 0);
	list->count--;
	entry->list = NULL;
	entry->prev->next = entry->next;
	entry->next->prev = entry->prev;
}

void *linkedlist_head(struct linkedlist *list)
{
	void *ret = NULL;
	linkedlist_lock(list);
	if(list->head->next != &list->sentry)
		ret = list->head->next->obj;
	linkedlist_unlock(list);
	return ret;
}

void *linkedlist_remove_head(struct linkedlist *list)
{
	void *ret = NULL;
	linkedlist_lock(list);
	if(list->head->next != &list->sentry) {
		ret = list->head->next->obj;
		linkedlist_do_remove(list, list->head->next);
	}
	linkedlist_unlock(list);
	return ret;
}

void *linkedlist_remove_tail(struct linkedlist *list)
{
	void *ret = NULL;
	linkedlist_lock(list);
	if(list->head->prev != &list->sentry) {
		ret = list->head->prev->obj;
		linkedlist_do_remove(list, list->head->prev);
	}
	linkedlist_unlock(list);
	return ret;
}

void linkedlist_create(struct linkedlist *list, int flags)
{
	list->lock = SPINLOCK_INIT;
	list->flags = flags;
	list->head = &list->sentry;
	list->head->next = list->head;
	list->head->prev = list->head;
	list->count = 0;
}

void linkedlist_insert(struct linkedlist *list, struct linkedentry *entry, void *obj)
{
	assert(list->head == &list->sentry);
	assert(list->head->next && list->head->prev);
	assert(list->count >= 0);
	linkedlist_lock(list);
	entry->next = list->head->next;
	entry->prev = list->head;
	entry->prev->next = entry;
	entry->next->prev = entry;
	entry->obj = obj;
	entry->list = list;
	list->count++;
	assert(list->count > 0);
	linkedlist_unlock(list);
}

void linkedlist_remove(struct linkedlist *list, struct linkedentry *entry)
{
	assert(list->head == &list->sentry);
	linkedlist_lock(list);
	linkedlist_do_remove(list, entry);
	linkedlist_unlock(list);
}

struct linkedentry *linkedlist_find(struct linkedlist *list, bool (*fn)(struct linkedentry *, void *data), void *data)
{
    linkedlist_lock(list);
    struct linkedentry *ent = list->head->next;
    while(ent != &list->sentry) {
        if(fn(ent, data))
            break;
        ent = ent->next;
    }
    linkedlist_unlock(list);
    if(ent == &list->sentry)
        return NULL;
    return ent;
}

