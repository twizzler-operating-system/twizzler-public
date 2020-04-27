#pragma once

#include <krc.h>
#include <spinlock.h>

#define PAGE_MAGIC 0x445566aaccddf15a

struct page {
#if DO_PAGE_MAGIC
	uint64_t page_magic;
#endif
	uintptr_t addr;
	struct spinlock lock;
	struct page *parent, *next;
	// struct rbroot root;
	// struct rbnode node;
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
#define PAGE_CRITICAL 0x40
#define PAGE_NOPHYS 0x80

struct page *page_alloc(int type, int flags, int level);
struct page *page_alloc_nophys(void);
void page_unpin(struct page *page);
void page_pin(struct page *page);
struct memregion;
void page_init(struct memregion *region);
void page_init_bootstrap(void);
void page_print_stats(void);
void page_dealloc(struct page *p, int flags);
void page_idle_zero(void);
void page_reset_crit_flag(bool v);
bool page_set_crit_flag(void);
