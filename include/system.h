#pragma once

#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

typedef long ssize_t;

static inline unsigned long long __round_up_pow2(unsigned int a)
{
	return ((a & (a - 1)) == 0) ? a : 1ull << (sizeof(a) * 8 - __builtin_clz(a));
}

#define __orderedbefore(x) (x-1)
#define __orderedafter(x) (x+1)

#define __initializer __attribute__((used,constructor))
#define __orderedinitializer(x) __attribute__((used,constructor(x+3000)))

#define __cleanup(f) __attribute__((cleanup(f)))
#define ___concat(x,y) x##y
#define __concat(x,y) ___concat(x, y)

#define __get_macro2(_1,_2,NAME,...) NAME

#define stringify_define(x) stringify(x)
#define stringify(x) #x

