#include <debug.h>
#include <lib/iter.h>
#include <lib/list.h>
#include <rand.h>
#include <spinlock.h>
#include <twz/_err.h>

static DECLARE_LIST(sources);
static DECLARE_SPINLOCK(lock);

static size_t uses = 0;

int rand_getbytes(void *data, size_t len, int flags)
{
	if(flags != 0)
		return -EINVAL;
	spinlock_acquire_save(&lock);
	/* TODO: do something real here */
	if((uses % 1024) == 0) {
		char entropy[RANDSIZ];
		size_t count = 0;
		foreach(e, list, &sources) {
			struct entropy_source *es = list_entry(e, struct entropy_source, entry);
			ssize_t l = es->get(entropy + count, RANDSIZ - count);
			if(l > 0) {
				count += l;
				if(count >= RANDSIZ)
					break;
			}
		}
		if(count < RANDSIZ && uses == 0) {
			/* TODO: block and retry */
			panic("failed to get enough entropy on startup");
		}
		rand_csprng_reseed(entropy, RANDSIZ);
	}
	rand_csprng_get(data, len);
	uses++;
	spinlock_release_restore(&lock);
	return 0;
}

void rand_register_entropy_source(struct entropy_source *src)
{
	spinlock_acquire_save(&lock);
	list_insert(&sources, &src->entry);
	spinlock_release_restore(&lock);
	printk("[rand]: registered entropy source '%s'\n", src->name);
}
