#pragma once
#define PANIC_UNWIND 0x1
#define PANIC_CONTINUE 0x2
__attribute__((format(printf, 4, 5))) void __panic(const char *file,
  int linenr,
  int flags,
  const char *msg,
  ...);

#include <assert.h>

#define panic(msg, ...)                                                                            \
	({                                                                                             \
		__panic(__FILE__, __LINE__, PANIC_UNWIND, msg, ##__VA_ARGS__);                             \
		__builtin_unreachable();                                                                   \
	})
#define panic_continue(msg, ...)                                                                   \
	__panic(__FILE__, __LINE__, PANIC_UNWIND | PANIC_CONTINUE, msg, ##__VA_ARGS__)

#include <interrupt.h>
void kernel_debug_entry(void);

#if FEATURE_SUPPORTED_UNWIND
struct frame {
	uintptr_t pc, fp;
};
void debug_print_backtrace(void);
bool arch_debug_unwind_frame(struct frame *frame);
void debug_puts(char *);

#endif
