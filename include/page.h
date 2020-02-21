#pragma once

#include <krc.h>
#include <spinlock.h>

struct page {
	uintptr_t addr;
	struct krc rc;
	struct spinlock lock;
	struct page *parent, *next;
	struct rbroot root;
	struct rbnode node;
	uint16_t flags;
	uint8_t type;
	uint8_t level;
	_Atomic int32_t cowcount;
	// uint16_t flags : 10;
	// uint16_t type : 3;
	// uint16_t level : 3;
};

#define PAGE_TYPE_VOLATILE 0
#define PAGE_TYPE_PERSIST 1
#define PAGE_TYPE_MMIO 2

#define PAGE_CACHE_TYPE(p) ((p)->flags & 0x7)
#define PAGE_CACHE_WB 0
#define PAGE_CACHE_UC 1
#define PAGE_CACHE_WT 2
#define PAGE_CACHE_WC 3

#define PAGE_ALLOCED 0x10
#define PAGE_ZERO 0x20

struct page *page_alloc(int type, int flags, int level);
struct page *page_alloc_nophys(void);
void page_unpin(struct page *page);
void page_pin(struct page *page);
struct memregion;
void page_init(struct memregion *region);
void page_init_bootstrap(void);
void page_print_stats(void);
