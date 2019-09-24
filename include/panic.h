#pragma once
#define PANIC_UNWIND 0x1
#define PANIC_CONTINUE 0x2
__attribute__((format(printf, 4, 5))) void __panic(const char *file,
  int linenr,
  int flags,
  const char *msg,
  ...);

#define panic(msg, ...)                                                                            \
	({                                                                                             \
		__panic(__FILE__, __LINE__, PANIC_UNWIND, msg, ##__VA_ARGS__);                             \
		__builtin_unreachable();                                                                   \
	})
#define panic_continue(msg, ...)                                                                   \
	__panic(__FILE__, __LINE__, PANIC_UNWIND | PANIC_CONTINUE, msg, ##__VA_ARGS__)
