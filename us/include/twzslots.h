#pragma once

#define TWZSLOT_COREENTRY   0xfffbul
#define TWZSLOT_CORETEXT    0xfffcul
#define TWZSLOT_COREDATA    0xfffdul
#define TWZSLOT_THRD       0x10000ul

#define TWZSLOT_CVIEW      0x1fff0ul
#define TWZSLOT_FILES_BASE 0x1f000ul
#define TWZSLOT_STDIN      0x1f000ul
#define TWZSLOT_STDOUT     0x1f001ul
#define TWZSLOT_STDERR     0x1f002ul

#define TWZSLOT_MAX_SLOT    0x1fffful
#define TWZSLOT_ALLOC_END   0x1f000ul
#define TWZSLOT_ALLOC_START 0x10000ul
#define TWZSLOT_ALLOC_NUM   (TWZSLOT_ALLOC_END - TWZSLOT_ALLOC_START)

#define SLOT_TO_VIRT(s) (char *)((s) * 1024ul * 1024 * 1024)
#define VIRT_TO_SLOT(s) (size_t)((uintptr_t)(s) / (1024ul * 1024 * 1024))
