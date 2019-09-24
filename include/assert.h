#pragma once
#if CONFIG_DEBUG

#include <panic.h>

#define assert(cond)                                                                               \
	do {                                                                                           \
		if(!__builtin_expect(cond, 0))                                                             \
			panic("assertion failure: %s", #cond);                                                 \
	} while(0)
#define assertmsg(cond, msg, ...)                                                                  \
	do {                                                                                           \
		if(!__builtin_expect(cond, 0))                                                             \
			panic("assertion failure: %s -- " msg, #cond, ##__VA_ARGS__);                          \
	} while(0)

#else

#define assert(x)
#define assertmsg(x, m, ...)

#endif
