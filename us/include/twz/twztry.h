#pragma once

#ifdef __cplusplus
#error "this header is only available for C code; use C++ exceptions in C++ code"
#endif

#include <setjmp.h>

extern _Thread_local jmp_buf *_twz_jmp_buf;
extern _Thread_local void *_twz_excep_data;

#define twztry                                                                                     \
	{                                                                                              \
		jmp_buf _jb;                                                                               \
		jmp_buf *_pjb = _twz_jmp_buf;                                                              \
		_twz_jmp_buf = &_jb;                                                                       \
		int _fc;                                                                                   \
		switch((_fc = setjmp(_jb))) {                                                              \
			default:                                                                               \
				longjmp(*_pjb, _fc);                                                               \
			case 0:

#define twzcatch(f)                                                                                \
	break;                                                                                         \
	case((f) + 1):

#define twzcatch_data() _twz_excep_data

#define twztry_end                                                                                 \
	}                                                                                              \
	}
