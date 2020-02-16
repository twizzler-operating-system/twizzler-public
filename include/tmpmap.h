#pragma once

struct page;
void tmpmap_unmap_page(void *addr);
void *tmpmap_map_page(struct page *page);
