#include <stdbool.h>
#include <lib/bitmap.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/linkedlist.h>
#include <memory.h>
#include <string.h>
#include <debug.h>
#define IS_POWER2(x) ((x != 0) && ((x & (~x + 1)) == x))

#define MIN_PHYS_MEM PHYSICAL_MEMORY_START

#define MAX_ORDER 21
#define MIN_SIZE MM_BUDDY_MIN_SIZE
#define MAX_SIZE ((uintptr_t)MIN_SIZE << MAX_ORDER)
#define MEMORY_SIZE (MAX_SIZE)

#define NOT_FREE (-1)
struct spinlock pm_buddy_lock = SPINLOCK_INIT;
uint8_t *bitmaps[MAX_ORDER + 1];

struct linkedlist freelists[MAX_ORDER+1];
static char static_bitmaps[((MEMORY_SIZE / MIN_SIZE) / 8) * 2];
static bool inited = false;
static size_t num_allocated[MAX_ORDER + 1];

static _Atomic size_t free_memory = 0;
static _Atomic size_t total_memory = 0;

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

#include <printk.h>
static uintptr_t __do_pmm_buddy_allocate(size_t length)
{
	assert(inited);
	if(!IS_POWER2(length))
		panic("can only allocate in powers of 2 (not %lx)", length);
	if(length < MIN_SIZE)
		panic("length less than minimum size");
	if(length > MAX_SIZE) {
		panic("out of physical memory");
	}

	int order = min_possible_order(length);

	if(freelists[order].count == 0) {
		uintptr_t a = __do_pmm_buddy_allocate(length * 2);

		struct linkedentry *elem1 = (void *)(a + PHYSICAL_MAP_START);
		struct linkedentry *elem2 = (void *)(a + length + PHYSICAL_MAP_START);

		linkedlist_insert(&freelists[order], elem1, (void *)a);
		linkedlist_insert(&freelists[order], elem2, (void *)(a + length));
	}

	uintptr_t address = (uintptr_t)linkedlist_remove_head(&freelists[order]);
	int bit = address / length;
	assert(!bitmap_test(bitmaps[order], bit));
	bitmap_set(bitmaps[order], bit);
	num_allocated[order]++;

	return address;
}

static int deallocate(uintptr_t address, int order)
{
	assert(inited);
	if(order > MAX_ORDER)
		return -1;
	int bit = address / ((uintptr_t)MIN_SIZE << order);
	if(!bitmap_test(bitmaps[order], bit)) {
		return deallocate(address, order + 1);
	} else {
		uintptr_t buddy = address ^ ((uintptr_t)MIN_SIZE << order);
		int buddy_bit = buddy / ((uintptr_t)MIN_SIZE << order);
		bitmap_reset(bitmaps[order], bit);

		if(!bitmap_test(bitmaps[order], buddy_bit)) {
			struct linkedentry *elem = (void *)(buddy + PHYSICAL_MAP_START);
			linkedlist_remove(&freelists[order], elem);
			deallocate(buddy > address ? address : buddy, order + 1);
		} else {
			struct linkedentry *elem = (void *)(address + PHYSICAL_MAP_START);
			linkedlist_insert(&freelists[order], elem, (void *)address);
		}
		num_allocated[order]--;
		return order;
	}
}

uintptr_t pmm_buddy_allocate(size_t length)
{
	spinlock_acquire(&pm_buddy_lock);
	uintptr_t ret = __do_pmm_buddy_allocate(length);
	free_memory -= length;
	spinlock_release(&pm_buddy_lock);
	return ret;
}

void pmm_buddy_deallocate(uintptr_t address)
{
	if(address >= MIN_PHYS_MEM + MEMORY_SIZE)
		return;
	spinlock_acquire(&pm_buddy_lock);
	int order = deallocate(address, 0);
	if(order >= 0) {
		free_memory += MIN_SIZE << order;
		if(total_memory < free_memory)
			total_memory = free_memory;
	}
	spinlock_release(&pm_buddy_lock);
}

void pmm_buddy_init(void)
{
	uintptr_t start = (uintptr_t)static_bitmaps;
	long length = ((MEMORY_SIZE / MIN_SIZE) / (8));
	for(int i=0;i<=MAX_ORDER;i++) {
		bitmaps[i] = (uint8_t *)start;
		memset(bitmaps[i], ~0, length);
		linkedlist_create(&freelists[i], LINKEDLIST_LOCKLESS);
		start += length;
		length /= 2;
		num_allocated[i] = buddy_order_max_blocks(i);
	}
	inited = true;
}

