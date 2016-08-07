#pragma once

#include <lib/linkedlist.h>
#include <interrupt.h>

struct workqueue {
	struct linkedlist list;
};

struct task {
	void (*fn)(void *);
	void *data;
	struct linkedentry entry;
};

static inline void workqueue_create(struct workqueue *wq)
{
	linkedlist_create(&wq->list, 0);
}

static inline void workqueue_insert(struct workqueue *wq, struct task *task)
{
	interrupt_set_scope(false);
	linkedlist_insert(&wq->list, &task->entry, task);
}

static inline void workqueue_dowork(struct workqueue *wq)
{
	bool s = arch_interrupt_set(false);
	struct task *t = linkedlist_remove_tail(&wq->list);
	arch_interrupt_set(s);
	if(t) {
		t->fn(t->data);
	}
}

static inline bool workqueue_pending(struct workqueue *wq)
{
	return wq->list.count != 0;
}

