#pragma once
#define PANIC_UNWIND 0x1
__attribute__ ((format (printf, 4, 5))) _Noreturn void __panic(const char *file, int linenr, int flags, const char *msg, ...);

#define panic(msg, ...) __panic(__FILE__, __LINE__, PANIC_UNWIND, msg, ##__VA_ARGS__)

#if CONFIG_DEBUG

#define assert(cond) do { if(!__builtin_expect(cond, 0)) panic("assertion failure: %s", #cond); } while(0)
#define assertmsg(cond, msg, ...) do { if(!__builtin_expect(cond, 0)) panic(0, "assertion failure: %s -- " msg, #cond, ##__VA_ARGS__); } while(0)

#else

#define assert(x)
#define assertmsg(x, m, ...)

#endif

