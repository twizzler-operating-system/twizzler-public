#include <stdarg.h>
#include <stdio.h>

__attribute__((noreturn)) void libtwz_panic(const char *s, ...)
{
	va_list va;
	va_start(va, s);
	vfprintf(stderr, s, va);
	va_end(va);
	abort();
}
