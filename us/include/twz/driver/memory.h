#pragma once
struct memory_stats {
	_Atomic size_t pages_early_used;
	_Atomic size_t memalloc_nr_objects;
	_Atomic size_t memalloc_total;
	_Atomic size_t memalloc_used;
	_Atomic size_t memalloc_unfreed;
	_Atomic size_t memalloc_free;
	_Atomic size_t pmap_used;
	_Atomic size_t tmpmap_used;
};

#define PAGE_STATS_INFO_CRITICAL 1
#define PAGE_STATS_INFO_ZERO 2

struct page_stats {
	_Atomic size_t page_size;
	_Atomic uint64_t info;
	_Atomic size_t avail;
};

struct memory_stats_header {
	struct memory_stats stats;
	size_t nr_page_groups;
	struct page_stats page_stats[];
};

struct nv_header {
	int x;
};
