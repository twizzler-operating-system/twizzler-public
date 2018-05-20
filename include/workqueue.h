#pragma once

#include <lib/list.h>
#include <interrupt.h>
#include <spinlock.h>

struct workqueue {
	struct list list;
	struct spinlock lock;
};

struct task {
	void (*fn)(void *);
	void *data;
	struct list entry;
};

static inline void workqueue_create(struct workqueue *wq)
{
	list_init(&wq->list);
	wq->lock = SPINLOCK_INIT;
}

static inline void workqueue_insert(struct workqueue *wq, struct task *task, void (*fn)(void *), void *data)
{
	task->fn = fn;
	task->data = data;
	interrupt_set_scope(false);
	list_insert(&wq->list, &task->entry);
}

static inline void workqueue_dowork(struct workqueue *wq)
{
	bool s = arch_interrupt_set(false);
	struct list *e = list_dequeue(&wq->list);
	arch_interrupt_set(s);
	if(e) {
		struct task *t = list_entry(e, struct task, entry);
		t->fn(t->data);
	}
}

static inline bool workqueue_pending(struct workqueue *wq)
{
	return !list_empty(&wq->list);
}

