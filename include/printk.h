#pragma once
#include <stdarg.h>

__attribute__((format(printf, 1, 2))) int printk(const char *fmt, ...);
int vprintk(const char *fmt, va_list args);

#define PR128FMT "%#0lx%#016lx"
#define PR128FMTd "%#0lx:%#016lx"

#define PR128(x) (uint64_t)(x >> 64), (uint64_t)x
