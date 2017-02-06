#pragma once

#include <arch-syscall.h>
#include <stdarg.h>

static inline long __twz_syscall(long n, ...)
{
	va_list ap;
	long a, b, c, d, e, f;
	va_start(ap, n);
	a = va_arg(ap, long);
	b = va_arg(ap, long);
	c = va_arg(ap, long);
	d = va_arg(ap, long);
	e = va_arg(ap, long);
	f = va_arg(ap, long);
	va_end(ap);
	return _syscall6(n, a, b, c, d, e, f);
}

static inline long __twz_syscallg(long n, __int128 g, ...)
{
	va_list ap;
	long a, b, c, d;
	va_start(ap, g);
	a = va_arg(ap, long);
	b = va_arg(ap, long);
	c = va_arg(ap, long);
	d = va_arg(ap, long);
	va_end(ap);
	return _syscallg14(n, g, a, b, c, d);
}

#define __twzs_CAT( A, B ) A ## B
#define __twzs_SELECT( NAME, NUM ) __twzs_CAT( NAME ## _, NUM )

#define __twzs_GET_COUNT( _1, _2, _3, _4, _5, _6, COUNT, ... ) COUNT
#define __twzs_VA_VERS( ... ) __twzs_GET_COUNT( __VA_ARGS__, 2, 2, 2, 2, 2, 1 )
#define __twzs_VA_SELECT( NAME, n, ... ) __twzs_SELECT( NAME, __twzs_VA_VERS(__VA_ARGS__) )(n, ##__VA_ARGS__)
#define twz_syscall( n, ... ) __twzs_VA_SELECT( __twzs_twz_syscall, n, ##__VA_ARGS__ )

#define __twzs_twz_syscall_1(n, ...) __twz_syscall(n, ##__VA_ARGS__)
#define __twzs_twz_syscall_2(n, f, ...) _Generic((f), __int128: __twz_syscallg, default: __twz_syscall)(n, f, ##__VA_ARGS__)

