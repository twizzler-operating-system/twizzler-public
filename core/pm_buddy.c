#include <stdbool.h>
#include <lib/bitmap.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/list.h>
#include <memory.h>
#include <string.h>
#include <debug.h>
#define IS_POWER2(x) ((x != 0) && ((x & (~x + 1)) == x))

#define NOT_FREE (-1)
static inline int min_possible_order(uintptr_t address)
{
	address /= MIN_SIZE;
	int o = 0;
	while(address && !(address & 1)) {
		o++;
		address >>= 1;
	}
	return o;
}

static inline size_t buddy_order_max_blocks(int order)
{
	return MEMORY_SIZE / ((uintptr_t)MIN_SIZE << order);
}

static uintptr_t __do_pmm_buddy_allocate(struct memregion *reg, size_t length)
{
	if(!IS_POWER2(length))
		panic("can only allocate in powers of 2 (not %lx)", length);
	if(length < MIN_SIZE)
		panic("length less than minimum size");
	if(length > MAX_SIZE) {
		panic("out of physical memory");
	}

	int order = min_possible_order(length);

	if(list_empty(&reg->freelists[order])) {
		uintptr_t a = __do_pmm_buddy_allocate(reg, length * 2);

		struct list *elem1 = mm_ptov(a);
		struct list *elem2 = mm_ptov(a + length);

		list_insert(&reg->freelists[order], elem1);
		list_insert(&reg->freelists[order], elem2);
	}

	uintptr_t address = mm_vtop(list_pop(&reg->freelists[order]));
	int bit = address / length;
	assert(!bitmap_test(reg->bitmaps[order], bit));
	bitmap_set(reg->bitmaps[order], bit);
	reg->num_allocated[order]++;

	return address;
}

static int deallocate(struct memregion *reg, uintptr_t address, int order)
{
	if(order > MAX_ORDER)
		return -1;
	int bit = address / ((uintptr_t)MIN_SIZE << order);
	if(!bitmap_test(reg->bitmaps[order], bit)) {
		return deallocate(reg, address, order + 1);
	} else {
		uintptr_t buddy = address ^ ((uintptr_t)MIN_SIZE << order);
		int buddy_bit = buddy / ((uintptr_t)MIN_SIZE << order);
		bitmap_reset(reg->bitmaps[order], bit);

		if(!bitmap_test(reg->bitmaps[order], buddy_bit)) {
			struct list *elem = mm_ptov(buddy);
			list_remove(elem);
			deallocate(reg, buddy > address ? address : buddy, order + 1);
		} else {
			struct list *elem = mm_ptov(address);
			list_insert(&reg->freelists[order], elem);
		}
		reg->num_allocated[order]--;
		return order;
	}
}

uintptr_t pmm_buddy_allocate(struct memregion *reg, size_t length)
{
	if(length < MIN_SIZE)
		length = MIN_SIZE;
	bool fl = spinlock_acquire(&reg->pm_buddy_lock);
	uintptr_t ret = __do_pmm_buddy_allocate(reg, length);
	reg->free_memory -= length;
	spinlock_release(&reg->pm_buddy_lock, fl);
	return ret;
}

void pmm_buddy_deallocate(struct memregion *reg, uintptr_t address)
{
	if(reg->ready) {
		spinlock_acquire_save(&reg->pm_buddy_lock);
	}
	int order = deallocate(reg, address, 0);
	if(order >= 0) {
		reg->free_memory += MIN_SIZE << order;
	}
	if(reg->ready) {
		spinlock_release_restore(&reg->pm_buddy_lock);
	}
}

void pmm_buddy_init(struct memregion *reg)
{
	uintptr_t start = (uintptr_t)reg->static_bitmaps;
	long length = ((MEMORY_SIZE / MIN_SIZE) / (8));
	for(int i=0;i<=MAX_ORDER;i++) {
		reg->bitmaps[i] = (uint8_t *)start;
		memset(reg->bitmaps[i], ~0, length);
		list_init(&reg->freelists[i]);
		start += length;
		length /= 2;
		reg->num_allocated[i] = buddy_order_max_blocks(i);
	}
}

