#include <clksrc.h>
#include <debug.h>
#include <spinlock.h>

static DECLARE_LIST(sources);
static DECLARE_SPINLOCK(lock);

static struct clksrc *best_monotonic = NULL;
static struct clksrc *best_countdown = NULL;

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

	if(best_countdown == NULL && (cs->flags & CLKSRC_INTERRUPT)) {
		best_countdown = cs;
		printk("[clk]: assigned 'best countdown' to %s\n", cs->name);
	} else {
		if((cs->flags & CLKSRC_INTERRUPT)
				&& (cs->precision < best_countdown->precision)) {
			best_countdown = cs;
			printk("[clk]: assigned 'best countdown' to %s\n", cs->name);
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

void clksrc_set_interrupt_countdown(uint64_t ns, bool periodic)
{
	if(!best_countdown || !clksrc_set_timer(best_countdown, ns, periodic)) {
		panic("no interrupt clock source available");
	}
}

bool clksrc_set_timer(struct clksrc *cs, uint64_t ns, bool periodic)
{
	if(!(cs->flags & CLKSRC_INTERRUPT) || !((cs->flags & CLKSRC_ONESHOT) || (cs->flags & CLKSRC_PERIODIC))) {
		return false;
	}
	if(periodic && !(cs->flags & CLKSRC_PERIODIC)) {
		return false;
	}
	if(!cs->set_timer) {
		return false;
	}
	cs->set_timer(cs, ns, periodic);
	return true;
}

void clksrc_set_active(struct clksrc *cs, bool active)
{
	if(cs->set_active) cs->set_active(cs, active);
}

