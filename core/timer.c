#include <time.h>
#include <thread.h>
#include <lib/list.h>
#include <lib/iter.h>

/* TODO (dbittman) [performance,minor]: change timer system to heap */

static DECLARE_LIST(timers);
static struct spinlock timers_list_lock = SPINLOCK_INIT;

static dur_nsec cur_time = 0;

void timer_add(struct timer *t, dur_nsec time, void (*fn)(void *), void *data)
{
	t->fn = fn;
	t->data = data;
	t->time = time + cur_time;
	interrupt_set_scope(false); /* prevent timer from firing while holding timer lock */
	spinlock_acquire_save(&timers_list_lock);
	foreach(e, list, &timers) {
		struct timer *et = list_entry(e, struct timer, entry);
		if(et->time > t->time) {
			list_insert(&et->entry, &t->entry);
			spinlock_release_restore(&timers_list_lock);
			return;
		}
	}
	list_insert(timers.prev, &t->entry);
	spinlock_release_restore(&timers_list_lock);
}

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

dur_nsec kernel_timer_tick(dur_nsec dt)
{
	cur_time += dt;
	check_timers();
	/* TODO: temp hack */
	return 10000000; //1 ms
}

