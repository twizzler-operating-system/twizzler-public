#pragma once
struct memory_stats {
	_Atomic size_t pages_total;
	_Atomic size_t pages_used;
	_Atomic size_t pages_early_used;
	_Atomic size_t kalloc_nr_objects;
	_Atomic size_t kalloc_total;
	_Atomic size_t kalloc_used;
	_Atomic size_t pmap_total;
	_Atomic size_t pmap_used;
	_Atomic size_t tmpmap_total;
	_Atomic size_t tmpmap_used;
};

struct page_stats {
	_Atomic size_t page_size;
	_Atomic size_t avail;
	_Atomic size_t avail_zero;
};

struct memory_stats_header {
	struct memory_stats stats;
	size_t nr_page_levels;
	struct page_stats page_stats[];
};
