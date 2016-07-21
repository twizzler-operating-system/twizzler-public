#include <memory.h>

void mm_init(void)
{
	pmm_buddy_init();

	struct linkedlist *regions = arch_mm_get_regions();
	for(struct linkedentry *entry = linkedlist_iter_start(regions);
			entry != linkedlist_iter_end(regions);
			entry = linkedlist_iter_next(entry)) {
		struct memregion *reg = linkedentry_obj(entry);

		printk("[mm]: memory region %lx -> %lx (%d KB), %x\n",
				reg->start, reg->start + reg->length, reg->length / 1024, reg->flags);

		for(uintptr_t addr = reg->start; addr < reg->start + reg->length;
				addr += MM_BUDDY_MIN_SIZE) {
			pmm_buddy_deallocate(addr);
		}
	}
}

