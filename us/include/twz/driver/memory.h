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

#include <twz/_objid.h>

struct nv_header {
	uint64_t devid;
	uint32_t regid;
	uint32_t flags;
	uint64_t meta_lo;
	uint64_t meta_hi;
};

#define NVD_HDR_MAGIC 0x12345678
struct nvdimm_region_header {
	uint32_t magic;
	uint32_t version;
	uint64_t flags;

	uint64_t total_pages;
	uint64_t used_pages;

	objid_t nameroot;

	uint32_t pg_used_num[];
};
