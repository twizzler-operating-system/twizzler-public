#pragma once

#include <page.h>

#define PMAP_WB PAGE_CACHE_WB
#define PMAP_UC PAGE_CACHE_UC
#define PMAP_WT PAGE_CACHE_WT
#define PMAP_WC PAGE_CACHE_WC

void *pmap_allocate(uintptr_t phys, int flags);
