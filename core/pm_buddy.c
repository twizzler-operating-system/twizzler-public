#include <stdbool.h>
#include <lib/bitmap.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/linkedlist.h>
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

	if(reg->freelists[order].count == 0) {
		uintptr_t a = __do_pmm_buddy_allocate(reg, length * 2);

		struct linkedentry *elem1 = (void *)(a + PHYSICAL_MAP_START);
		struct linkedentry *elem2 = (void *)(a + length + PHYSICAL_MAP_START);

		linkedlist_insert(&reg->freelists[order], elem1, (void *)a);
		linkedlist_insert(&reg->freelists[order], elem2, (void *)(a + length));
	}

	uintptr_t address = (uintptr_t)linkedlist_remove_head(&reg->freelists[order]);
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
			struct linkedentry *elem = (void *)(buddy + PHYSICAL_MAP_START);
			linkedlist_remove(&reg->freelists[order], elem);
			deallocate(reg, buddy > address ? address : buddy, order + 1);
		} else {
			struct linkedentry *elem = (void *)(address + PHYSICAL_MAP_START);
			linkedlist_insert(&reg->freelists[order], elem, (void *)address);
		}
		reg->num_allocated[order]--;
		return order;
	}
}

uintptr_t pmm_buddy_allocate(struct memregion *reg, size_t length)
{
	if(length < MIN_SIZE)
		length = MIN_SIZE;
	spinlock_acquire(&reg->pm_buddy_lock);
	uintptr_t ret = __do_pmm_buddy_allocate(reg, length);
	reg->free_memory -= length;
	spinlock_release(&reg->pm_buddy_lock);
	return ret;
}

void pmm_buddy_deallocate(struct memregion *reg, uintptr_t address)
{
	spinlock_acquire(&reg->pm_buddy_lock);
	int order = deallocate(reg, address, 0);
	if(order >= 0) {
		reg->free_memory += MIN_SIZE << order;
	}
	spinlock_release(&reg->pm_buddy_lock);
}

void pmm_buddy_init(struct memregion *reg)
{
	uintptr_t start = (uintptr_t)reg->static_bitmaps;
	long length = ((MEMORY_SIZE / MIN_SIZE) / (8));
	for(int i=0;i<=MAX_ORDER;i++) {
		reg->bitmaps[i] = (uint8_t *)start;
		memset(reg->bitmaps[i], ~0, length);
		linkedlist_create(&reg->freelists[i], LINKEDLIST_LOCKLESS);
		start += length;
		length /= 2;
		reg->num_allocated[i] = buddy_order_max_blocks(i);
	}
}

