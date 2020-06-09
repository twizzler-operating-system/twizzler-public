#pragma once

#include <interrupt.h>
#include <lib/list.h>
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

static inline void workqueue_insert(struct workqueue *wq,
  struct task *task,
  void (*fn)(void *),
  void *data)
{
	task->fn = fn;
	task->data = data;
	// interrupt_set_scope(false);
	spinlock_acquire_save(&wq->lock);
	list_insert(&wq->list, &task->entry);
	spinlock_release_restore(&wq->lock);
}

static inline void workqueue_dowork(struct workqueue *wq)
{
	// bool s = arch_interrupt_set(false);
	spinlock_acquire_save(&wq->lock);
	struct list *e = list_dequeue(&wq->list);
	spinlock_release_restore(&wq->lock);
	// arch_interrupt_set(s);
	if(e) {
		struct task *t = list_entry(e, struct task, entry);
		t->fn(t->data);
	}
}

static inline bool workqueue_pending(struct workqueue *wq)
{
	return !list_empty(&wq->list);
}
