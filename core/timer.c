#include <time.h>
#include <thread.h>
#include <lib/linkedlist.h>

/* TODO (dbittman) [performance,minor]: change timer system to heap */

static struct linkedlist timers;

static dur_nsec cur_time = 0;

__initializer static void _init_timers(void)
{
	linkedlist_create(&timers, 0);
}

static int _compar(void *a, void *b)
{
	struct timer *t1 = a;
	struct timer *t2 = b;
	/* we care about overflow */
	if(t2->time < t1->time) return 1;
	if(t2->time > t1->time) return -1;
	return 0;
}

void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data)
{
	t->fn = fn;
	t->data = data;
	t->time = time + cur_time;
	interrupt_set_scope(false); /* prevent timer from firing while holding timer lock */
	linkedlist_insert_sorted(&timers, _compar, &t->entry, t);
}

static void check_timers(void)
{
	linkedlist_lock(&timers);
	struct linkedentry *entry = linkedlist_iter_start(&timers);
	if(entry != linkedlist_iter_end(&timers)) {
		struct timer *t = linkedentry_obj(entry);
		if(t->time < cur_time) {
			__linkedlist_remove_unlocked(&timers, &t->entry);
			linkedlist_unlock(&timers);
			t->fn(t->data);
			return;
		}
	}
	linkedlist_unlock(&timers);
}

dur_nsec kernel_timer_tick(dur_nsec dt)
{
	cur_time += dt;
	check_timers();
	/* TODO: temp hack */
	return 10000000; //1 ms
}

