#pragma once

#include <immintrin.h>

#define __CL_SIZE 64

#ifdef __CLWB__
#define __HAVE_CLWB
#endif

#define __FM_UNKNOWN 0
#define __FM_CLFLUSH 1
#define __FM_CLFLUSH_OPT 2
#define __FM_CLWB 3

#pragma GCC push_options
#pragma GCC target("clflushopt")
#pragma GCC target("clwb")

#include <cpuid.h>

#include <twz/debug.h>
static inline void _clwb(const void *p)
{
	static int __flush_mode = __FM_UNKNOWN;
	if(__flush_mode == __FM_UNKNOWN) {
		uint32_t a, b, c, d;
		if(__get_cpuid_count(7, 0, &a, &b, &c, &d)) {
			if(b & bit_CLWB) {
				__flush_mode = __FM_CLWB;
			} else if(b & bit_CLFLUSHOPT) {
				__flush_mode = __FM_CLFLUSH_OPT;
			} else {
				__flush_mode = __FM_CLFLUSH;
			}
		} else {
			/* fall-back to clflush... hopefully it's present. */
			__flush_mode = __FM_CLFLUSH;
		}
	}
	switch(__flush_mode) {
		case __FM_CLFLUSH:
			_mm_clflush(p);
			break;
		case __FM_CLFLUSH_OPT:
			_mm_clflushopt(p);
			break;
		case __FM_CLWB:
			_mm_clwb(p);
	}
}

static inline void _clwb_len(const void *p, size_t len)
{
	char *l = p;
	long long rem = len;
	while(rem > 0) {
		_clwb(l);
		size_t off = (uintptr_t)l & (__CL_SIZE - 1);
		l += (__CL_SIZE - off);
		rem -= (__CL_SIZE - off);
	}
}

static inline void _pfence(void)
{
	asm volatile("sfence;" ::: "memory");
}

#undef __FM_UNKNOWN
#undef __FM_CLFLUSH
#undef __FM_CLFLUSH_OPT
#undef __FM_CLWB
#undef __HAVE_CLWB

#pragma GCC pop_options
