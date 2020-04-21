#pragma once
#include <assert.h>
#include <interrupt.h>
#include <panic.h>
void kernel_debug_entry(void);

#if FEATURE_SUPPORTED_UNWIND
struct frame {
	uintptr_t pc, fp;
};
void debug_print_backtrace(void);
bool arch_debug_unwind_frame(struct frame *frame);
void debug_puts(char *);

#endif

#if CONFIG_INSTRUMENT
void kernel_instrument_start(void);
#endif
