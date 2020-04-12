#pragma once

#include <twz/_obj.h>

#define TWZSLOT_CVIEW 0x1fff0ul
#define TWZSLOT_MAX_SLOT 0x1fffful

#define TWZSLOT_THRD 0x10000
#define TWZSLOT_STACK 0x10001
#define TWZSLOT_UNIX 0x10002

#define TWZSLOT_ALLOC_START 0x10010
#define TWZSLOT_ALLOC_MAX 0x19fff

#define TWZSLOT_FILES_BASE 0x1a000

#ifndef __KERNEL__
#define SLOT_TO_VADDR(s) ({ (void *)((s)*OBJ_MAXSIZE); })
#define VADDR_TO_SLOT(s) ({ (size_t)((uintptr_t)(s) / OBJ_MAXSIZE); })
#endif
