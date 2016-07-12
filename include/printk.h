#pragma once
#include <stdarg.h>

__attribute__ ((format (printf, 1, 2))) int printk(const char *fmt, ...);
int vprintk(const char *fmt, va_list args);
