#include <clksrc.h>
#include <lib/iter.h>
#include <lib/list.h>
#include <processor.h>
#include <thread.h>
#include <time.h>

/* TODO (perf): change timer system to heap */

static DECLARE_PER_CPU(struct rbroot, timer_root) = RBINIT;
static DECLARE_PER_CPU(struct spinlock, timer_lock) = SPINLOCK_INIT;

static int __timer_compar_key(struct timer *a, dur_nsec b)
{
	if(a->time > b)
		return 1;
	else if(a->time < b)
		return -1;
	/* timers can never be equal. Break ties. */
	return 1;
}

static int __timer_compar(struct timer *a, struct timer *b)
{
	return __timer_compar_key(a, b->time);
}

void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data)
{
	struct rbroot *timer_root = per_cpu_get(timer_root);
	struct spinlock *timer_lock = per_cpu_get(timer_lock);
	t->fn = fn;
	t->data = data;
	t->time = time + clksrc_get_nanoseconds();
	spinlock_acquire_save(timer_lock);
	if(!t->active) {
		rb_insert(timer_root, t, struct timer, node, __timer_compar);
		t->active = true;
	}
	spinlock_release_restore(timer_lock);
}

void timer_remove(struct timer *t)
{
	struct rbroot *timer_root = per_cpu_get(timer_root);
	struct spinlock *timer_lock = per_cpu_get(timer_lock);
	spinlock_acquire_save(timer_lock);
	if(t->active) {
		rb_delete(&t->node, timer_root);
		t->active = false;
	}
	spinlock_release_restore(timer_lock);
}

uint64_t timer_check_timers(void)
{
	uint64_t ret = 0;
	struct rbroot *timer_root = per_cpu_get(timer_root);
	struct spinlock *timer_lock = per_cpu_get(timer_lock);
	spinlock_acquire_save(timer_lock);
	struct rbnode *node = rb_first(timer_root);
	if(node) {
		struct timer *t = rb_entry(node, struct timer, node);
		assert(t->active);
		uint64_t now = clksrc_get_nanoseconds();
		if(now >= t->time) {
			rb_delete(&t->node, timer_root);
			t->active = false;
			t->fn(t->data);
		} else {
			ret = t->time - now;
		}
		spinlock_release_restore(timer_lock);
	} else {
		spinlock_release_restore(timer_lock);
	}
	return ret;
}

#if 0
static void check_timers(void)
{
	spinlock_acquire_save(&timers_list_lock);
	struct list *entry = list_iter_start(&timers);
	if(entry != list_iter_end(&timers)) {
		struct timer *t = list_entry(entry, struct timer, entry);
		if(t->time < cur_time) {
			list_remove(&t->entry);
			spinlock_release_restore(&timers_list_lock);
			t->fn(t->data);
			return;
		}
	}
	spinlock_release_restore(&timers_list_lock);
}
#endif
