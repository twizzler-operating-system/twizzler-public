#pragma once

struct page;
void tmpmap_unmap_page(void *addr);
void *tmpmap_map_page(struct page *page);
struct memory_stats;
void tmpmap_collect_stats(struct memory_stats *stats);
