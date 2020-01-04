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

static uintptr_t __do_pmm_buddy_allocate(struct memregion *reg, size_t length)
{
	if(!IS_POWER2(length))
		panic("can only allocate in powers of 2 (not %lx)", length);
	if(length < MIN_SIZE)
		panic("length less than minimum size");
	if(length > reg->length - reg->alloc->off) {
		return (uintptr_t)~0ull;
	}

	int order = min_possible_order(length);
	// printk("pmm_alloc: %lx (%d)\n", length, order);

	if(list_empty(&reg->alloc->freelists[order])) {
		uintptr_t a = __do_pmm_buddy_allocate(reg, length * 2);

		struct list *elem1 = mm_ptov(a + reg->start + reg->alloc->off);
		struct list *elem2 = mm_ptov(a + length + reg->start + reg->alloc->off);

		list_insert(&reg->alloc->freelists[order], elem1);
		list_insert(&reg->alloc->freelists[order], elem2);
	}

	uintptr_t address =
	  mm_vtop(list_pop(&reg->alloc->freelists[order])) - (reg->start + reg->alloc->off);
	int bit = address / length;
	assert(!bitmap_test(reg->alloc->bitmaps[order], bit));
	bitmap_set(reg->alloc->bitmaps[order], bit);
	reg->alloc->num_allocated[order]++;

	return address;
}

static int deallocate(struct memregion *reg, uintptr_t address, int order)
{
	if(order > MAX_ORDER)
		return -1;
	int bit = address / ((uintptr_t)MIN_SIZE << order);
	if(!bitmap_test(reg->alloc->bitmaps[order], bit)) {
		return deallocate(reg, address, order + 1);
	} else {
		uintptr_t buddy = address ^ ((uintptr_t)MIN_SIZE << order);
		int buddy_bit = buddy / ((uintptr_t)MIN_SIZE << order);
		bitmap_reset(reg->alloc->bitmaps[order], bit);

		if(!bitmap_test(reg->alloc->bitmaps[order], buddy_bit)) {
			struct list *elem = mm_ptov(buddy + (reg->start + reg->alloc->off));
			list_remove(elem);
			deallocate(reg, buddy > address ? address : buddy, order + 1);
		} else {
			struct list *elem = mm_ptov(address + reg->start + reg->alloc->off);
			list_insert(&reg->alloc->freelists[order], elem);
		}
		reg->alloc->num_allocated[order]--;
		return order;
	}
}

uintptr_t pmm_buddy_allocate(struct memregion *reg, size_t length)
{
	if(length < MIN_SIZE)
		length = MIN_SIZE;
	bool fl = spinlock_acquire(&reg->alloc->pm_buddy_lock);
	// printk("allocate from region: %lx->%lx (%lx); %lx length %lx\n",
	// reg->start,
	// reg->start + reg->length,
	// reg->length,
	// reg->alloc->free_memory,
	// length);
	uintptr_t x = __do_pmm_buddy_allocate(reg, length);
	if(x != (uintptr_t)~0)
		x += reg->start + reg->alloc->off;
	else
		x = 0;
	reg->alloc->free_memory -= length;
	spinlock_release(&reg->alloc->pm_buddy_lock, fl);
	return x;
}

void pmm_buddy_deallocate(struct memregion *reg, uintptr_t address)
{
	if(reg->alloc->ready) {
		spinlock_acquire_save(&reg->alloc->pm_buddy_lock);
	}
	int order = deallocate(reg, address - (reg->start + reg->alloc->off), 0);
	if(order >= 0) {
		reg->alloc->free_memory += MIN_SIZE << order;
	}
	if(reg->alloc->ready) {
		spinlock_release_restore(&reg->alloc->pm_buddy_lock);
	}
}

void pmm_buddy_init(struct memregion *reg)
{
	// reg->alloc->off = mm_page_size(1) - (reg->start % mm_page_size(1));
	// if(reg->alloc->off >= reg->length)
	//	return;
	/* TODO: make the skipped over memory available */
	char *start = reg->alloc->static_bitmaps = mm_ptov(reg->start + reg->alloc->off);
	size_t bmlen = (((reg->length - reg->alloc->off) / MIN_SIZE) / 8) * 2
	               + MAX_ORDER * 2; /* 1/2 + 1/4 + 1/8 ... */
	long length = (((reg->length - reg->alloc->off) / MIN_SIZE) / (8));
	for(int i = 0; i <= MAX_ORDER; i++) {
		reg->alloc->bitmaps[i] = (uint8_t *)start;
		memset(reg->alloc->bitmaps[i], ~0, length + 2);
		list_init(&reg->alloc->freelists[i]);
		start += length + 2;
		length /= 2;
		reg->alloc->num_allocated[i] = buddy_order_max_blocks(reg->length - reg->alloc->off, i);
	}
	for(uintptr_t addr = reg->start + reg->alloc->off + align_up(bmlen, MIN_SIZE);
	    addr < reg->start + reg->alloc->off + (reg->length - reg->alloc->off);
	    addr += MIN_SIZE) {
		pmm_buddy_deallocate(reg, addr);
	}
	reg->alloc->ready = true;
}
