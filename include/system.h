#pragma once

#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

typedef long ssize_t;

static inline unsigned long long __round_up_pow2(unsigned int a)
{
	return ((a & (a - 1)) == 0) ? a : 1ull << (sizeof(a) * 8 - __builtin_clz(a));
}

