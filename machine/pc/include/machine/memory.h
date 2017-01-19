#pragma once
#define KERNEL_LOAD_OFFSET 0x400000
#define KERNEL_PHYSICAL_BASE 0x0
#define KERNEL_VIRTUAL_BASE 0xFFFFFFFF80000000
#define PHYSICAL_MAP_START 0xFFFFFFC000000000

#ifndef __ASSEMBLER__
#include <debug.h>
static inline size_t arch_mm_page_size(int level)
{
	switch(level) {
		case 0: return 0x1000;
		case 1: return 2 * 1024 * 1024;
		case 2: return 1024 * 1024 * 1024;
		default: panic("invalid page-level %d", level);
	}
}

#endif
