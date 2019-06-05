#pragma once

#include <stdarg.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static const char *DIGITS_lower = "0123456789abcdef";
static const char *DIGITS_upper = "0123456789ABCDEF";
#define WN_LOWER 1
#define WN_NONEG 2
#define WN_LJUST 4
static char *write_number(char *buffer,
  long long value,
  int base,
  int options,
  int min_width,
  int precision)
{
	char digits[256];
	memset(digits, 0, 256);
	bool negative = false;
	unsigned long long tmp;
	if(value < 0 && !(options & WN_NONEG)) {
		negative = true;
		tmp = -value;
	} else {
		tmp = value;
	}

	int len = 0;
	while(tmp) {
		digits[len] = tmp % base;
		tmp /= base;
		len++;
	}

	if(!len)
		len = 1;

	if(negative)
		*buffer++ = '-';
	if(!(options & WN_LJUST)) {
		for(int i = 0; i + ((len > precision) ? len : precision) < min_width; i++)
			*buffer++ = ' ';
	}
	if(base == 16) {
		*buffer++ = '0';
		*buffer++ = 'x';
	} else if(base == 2) {
		*buffer++ = '0';
		*buffer++ = 'b';
	}

	for(int i = len; i < precision; i++)
		*buffer++ = '0';

	for(int i = len - 1; i >= 0; i--) {
		if(options & WN_LOWER)
			*buffer++ = DIGITS_lower[(int)digits[i]];
		else
			*buffer++ = DIGITS_upper[(int)digits[i]];
	}

	if(options & WN_LJUST) {
		for(int i = 0; i + ((len > precision ? len : precision)) < min_width; i++)
			*buffer++ = ' ';
	}

	return buffer;
}

#define GETVAL(value, sign)                                                                        \
	do {                                                                                           \
		if(l == 0)                                                                                 \
			value = va_arg(args, sign int);                                                        \
		else if(l == 1)                                                                            \
			value = va_arg(args, sign long);                                                       \
		else                                                                                       \
			value = va_arg(args, sign long long);                                                  \
	} while(false)

#define isnum(n) (n >= '0' && n <= '9')
static int parse_number(const char **str)
{
	const char *tmp = *str;
	int num = 0;
	while(isnum(*tmp)) {
		num = num * 10 + *tmp - '0';
		tmp++;
	}
	*str = tmp;
	return num;
}

static void vbufprintk(char *buffer, const char *fmt, va_list args)
{
	const char *s = fmt;
	char *b = buffer;
	while(*s) {
		if(*s == '%') {
			s++;
			int flags = 0;
			if(*s == '-') {
				s++;
				flags |= WN_LJUST;
			}
			/* next up, look for min field width */
			unsigned int min_field_width = parse_number(&s);
			if(*s == '*') {
				s++;
				min_field_width = va_arg(args, int);
			}
			unsigned int precision = 0;
			if(*s == '.') {
				s++;
				precision = parse_number(&s);
			}
			char type = *s;
			int l = 0;
			if(type == 'l') {
				type = *(++s);
				l++;
			}
			/* do it a second time... */
			if(type == 'l') {
				type = *(++s);
				l++;
			}

			switch(type) {
				char *str;
				long long value;
				unsigned long long uvalue;
				case 0:
					goto done;
				case 'd':
				case 'i':
					GETVAL(value, signed);
					b = write_number(b, value, 10, flags, min_field_width, precision);
					break;
				case 'u':
					GETVAL(uvalue, unsigned);
					b = write_number(b, uvalue, 10, flags | WN_NONEG, min_field_width, precision);
					break;
				case 'o':
					GETVAL(uvalue, unsigned);
					b = write_number(b, uvalue, 8, flags | WN_NONEG, min_field_width, precision);
					break;
				case 'c':
					*b++ = (unsigned char)va_arg(args, int);
					break;
				case 'x':
					GETVAL(uvalue, unsigned);
					b = write_number(b, uvalue, 16, flags | WN_NONEG, min_field_width, precision);
					break;
				case 'b':
					GETVAL(uvalue, unsigned);
					b = write_number(b, uvalue, 2, flags | WN_NONEG, min_field_width, precision);
					break;
				case 'p':
					uvalue = (unsigned long long)va_arg(args, void *);
					b = write_number(b, uvalue, 16, flags | WN_NONEG, min_field_width, precision);
					break;
				case 's':
					str = va_arg(args, char *);
					if(!str)
						str = "(null)";
					size_t len = strlen(str);
					if(precision && precision < len)
						len = precision;
					if(!(flags & WN_LJUST)) {
						for(int i = 0; len + i < min_field_width; i++)
							*b++ = ' ';
					}
					unsigned int count = 0;
					while(*str && count++ < len)
						*b++ = *str++;
					if(flags & WN_LJUST) {
						for(int i = 0; len + i < min_field_width; i++)
							*b++ = ' ';
					}
					break;
				case '%':
					*b++ = '%';
			}
		} else {
			*b++ = *s;
		}
		s++;
	}
done:
	*b = 0;
}

static inline long __twz__syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	unsigned long ret;
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5;
	register long r10 __asm__("r10") = a6;
	__asm__ __volatile__("syscall;"
	                     : "=a"(ret)
	                     : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9), "r"(r10)
	                     : "r11", "rcx", "memory");
	return ret;
}

#define SYS_DEBUG_PRINT 2
static inline long __debug_print(const char *str, size_t len)
{
	return __twz__syscall6(SYS_DEBUG_PRINT, (long)str, len, 0, 0, 0, 0);
}

__attribute__((used)) static int debug_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buf[1024];
	for(int i = 0; i < 1024; i++)
		buf[i] = 0;
	vbufprintk(buf, fmt, args);
	__debug_print(buf, strlen(buf));
	va_end(args);
	return 0;
}

__attribute__((used)) static int debug_vprintf(const char *fmt, va_list args)
{
	char buf[1024];
	for(int i = 0; i < 1024; i++)
		buf[i] = 0;
	vbufprintk(buf, fmt, args);
	__debug_print(buf, strlen(buf));
	return 0;
}
