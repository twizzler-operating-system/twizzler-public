#pragma once

struct list {
	struct list *next, *prev;
};

#define DECLARE_LIST(name) struct list name = { &name, &name }

static inline void list_init(struct list *l)
{
	l->prev = l;
	l->next = l;
}

#define list_empty(l) ((l)->next == (l))

static inline void list_insert(struct list *list, struct list *entry)
{
	entry->prev = list;
	entry->next = list->next;
	entry->prev->next = entry;
	entry->next->prev = entry;
}

static inline void list_remove(struct list *entry)
{
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
}

static inline struct list *list_pop(struct list *l)
{
	struct list *next = l->next;
	list_remove(next);
	return next == l ? NULL : next;
}

static inline struct list *list_dequeue(struct list *l)
{
	struct list *prev = l->prev;
	list_remove(prev);
	return prev == l ? NULL : prev;
}

#define list_entry(e, type, memb) \
	container_of(e, type, memb)

#define list_entry_next(item, memb) \
	list_entry((item)->memb.next, typeof(*(item)), memb)

#define list_entry_prev(item, memb) \
	list_entry((item)->memb.prev, typeof(*(item)), memb)

#define list_iter_start(list) \
	(list)->next

#define list_iter_end(list) list

#define list_iter_next(e) (e)->next
#define list_iter_prev(e) (e)->prev

