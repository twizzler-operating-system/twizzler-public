#include <clksrc.h>
#include <debug.h>
#include <spinlock.h>

static DECLARE_LIST(sources);
static DECLARE_SPINLOCK(lock);

static struct clksrc *best_monotonic = NULL;

void clksrc_register(struct clksrc *cs)
{
	printk("[clk]: registered '%s': flags=%lx, period=%ldps, prec=%ldns, rtime=%ldns\n",
			cs->name, cs->flags, cs->period_ps, cs->precision, cs->read_time);
	spinlock_acquire_save(&lock);
	list_insert(&sources, &cs->entry);
	if(best_monotonic == NULL && (cs->flags & CLKSRC_MONOTONIC)) {
		best_monotonic = cs;
		printk("[clk]: assigned 'best monotonic' to %s\n", cs->name);
	} else {
		if((cs->flags & CLKSRC_MONOTONIC)
				&& (cs->read_time < best_monotonic->read_time)) {
			best_monotonic = cs;
			printk("[clk]: assigned 'best monotonic' to %s\n", cs->name);
		}
	}
	spinlock_release_restore(&lock);
}

__noinstrument
uint64_t clksrc_get_nanoseconds(void)
{
	if(best_monotonic == NULL) {
		panic("no monotonic clock source available");
	}
	uint64_t cnt = best_monotonic->read_counter(best_monotonic);
	return (cnt * best_monotonic->period_ps) / 1000;
}

