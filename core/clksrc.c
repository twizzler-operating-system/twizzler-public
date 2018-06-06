#include <clksrc.h>
#include <spinlock.h>

static DECLARE_LIST(sources);
static DECLARE_SPINLOCK(lock);

void clksrc_register(struct clksrc *cs)
{
	spinlock_acquire_save(&lock);
	list_insert(&sources, &cs->entry);
	spinlock_release_restore(&lock);

	printk("[clk]: registered '%s': flags=%lx, period=%ldps, prec=%ldns, rtime=%ldns\n",
			cs->name, cs->flags, cs->period_ps, cs->precision, cs->read_time);
}

void clksrc_deregister(struct clksrc *cs)
{
	spinlock_acquire_save(&lock);
	list_remove(&cs->entry);
	spinlock_release_restore(&lock);
}

