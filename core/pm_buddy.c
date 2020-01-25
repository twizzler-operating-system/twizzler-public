#include <debug.h>
#include <lib/bitmap.h>
#include <lib/list.h>
#include <memory.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define IS_POWER2(x) ((x != 0) && ((x & (~x + 1)) == x))

#define NOT_FREE (-1)

#if 1

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

static inline size_t buddy_order_max_blocks(size_t reglen, int order)
{
	return reglen / ((uintptr_t)MIN_SIZE << order);
}

static uintptr_t __do_pmm_buddy_allocate(struct mem_allocator *reg, size_t length)
{
	if(!IS_POWER2(length))
		panic("can only allocate in powers of 2 (not %lx)", length);
	if(length < MIN_SIZE)
		panic("length less than minimum size");
	if(length > reg->length) {
		return (uintptr_t)~0ull;
	}

	int order = min_possible_order(length);
	printk("pmm_alloc: %lx (%d)\n", length, order);

	if(list_empty(&reg->freelists[order])) {
		uintptr_t a = __do_pmm_buddy_allocate(reg, length * 2);

		struct list *elem1 = (void *)(a + (char *)reg->vstart);
		struct list *elem2 = (void *)(a + length + (char *)reg->vstart);

		list_insert(&reg->freelists[order], elem1);
		list_insert(&reg->freelists[order], elem2);
	}

	uintptr_t address = ((uintptr_t)list_pop(&reg->freelists[order])) - (uintptr_t)(reg->vstart);
	int bit = address / length;
	assert(!bitmap_test(reg->bitmaps[order], bit));
	bitmap_set(reg->bitmaps[order], bit);
	reg->num_allocated[order]++;

	return address;
}

static int deallocate(struct mem_allocator *reg, uintptr_t address, int order)
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
			struct list *elem = (void *)(buddy + (char *)(reg->vstart));
			printk("remove %d\n", order);
			list_remove(elem);
			deallocate(reg, buddy > address ? address : buddy, order + 1);
		} else {
			struct list *elem = (void *)(address + (char *)reg->vstart);
			printk("insert %d\n", order);
			list_insert(&reg->freelists[order], elem);
		}
		reg->num_allocated[order]--;
		return order;
	}
}

uintptr_t pmm_buddy_allocate(struct mem_allocator *reg, size_t length)
{
	if(length < MIN_SIZE)
		length = MIN_SIZE;
	bool fl = spinlock_acquire(&reg->pm_buddy_lock);
	// printk("allocate from region: %lx->%lx (%lx); %lx length %lx\n",
	// reg->start,
	// reg->start + reg->length,
	// reg->length,
	// reg->alloc->free_memory,
	// length);
	uintptr_t x = __do_pmm_buddy_allocate(reg, length);
	printk(":: %lx\n", x);
	if(x != (uintptr_t)~0)
		x += (uintptr_t)reg->vstart;
	else {
		panic("X");
		x = 0;
	}
	reg->free_memory -= length;
	spinlock_release(&reg->pm_buddy_lock, fl);
	return x;
}

void pmm_buddy_deallocate(struct mem_allocator *reg, uintptr_t address)
{
	if(reg->ready) {
		spinlock_acquire_save(&reg->pm_buddy_lock);
	}
	int order = deallocate(reg, address - (uintptr_t)(reg->vstart), 0);
	if(order >= 0) {
		reg->free_memory += MIN_SIZE << order;
	}
	if(reg->ready) {
		spinlock_release_restore(&reg->pm_buddy_lock);
	}
}

void pmm_buddy_init(struct mem_allocator *alloc)
{
	char *start = alloc->static_bitmaps = (char *)alloc->vstart;
	size_t bmlen = (((alloc->length) / MIN_SIZE) / 8) * 2 + MAX_ORDER * 2; /* 1/2 + 1/4 + 1/8 ... */
	long length = (((alloc->length) / MIN_SIZE) / (8));
	printk("Init\n");
	for(int i = 0; i <= MAX_ORDER; i++) {
		alloc->bitmaps[i] = (uint8_t *)start;
		memset(alloc->bitmaps[i], ~0, length + 2);
		list_init(&alloc->freelists[i]);
		start += length + 2;
		length /= 2;
		alloc->num_allocated[i] = buddy_order_max_blocks(alloc->length, i);
	}
	for(char *addr = alloc->vstart + align_up(bmlen, MIN_SIZE);
	    addr < alloc->vstart + (alloc->length);
	    addr += MIN_SIZE) {
		printk("dealloc: %lx\n", addr);
		pmm_buddy_deallocate(alloc, (uintptr_t)addr);
	}
	alloc->ready = true;
}

#endif
