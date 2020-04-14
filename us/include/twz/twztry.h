#pragma once

#ifdef __cplusplus
#error "this header is only available for C code; use C++ exceptions in C++ code"
#endif

#include <setjmp.h>

extern _Thread_local jmp_buf *_Atomic _twz_jmp_buf;
extern _Thread_local void *_Atomic _twz_excep_data;

extern void _twz_try_unhandled(int, void *);

#define twztry                                                                                     \
	{                                                                                              \
		jmp_buf _jb;                                                                               \
		jmp_buf *_pjb = _twz_jmp_buf;                                                              \
		_twz_jmp_buf = &_jb;                                                                       \
		int _fc, _hdl = 0;                                                                         \
		switch((_fc = setjmp(_jb))) {                                                              \
			case 0:

#define twzcatch(f)                                                                                \
	_hdl = 1;                                                                                      \
	break;                                                                                         \
	case((f) + 1):                                                                                 \
		_hdl = 1;

#define twzcatch_data() _twz_excep_data
#define twzcatch_fnum() ({ _fc - 1; })

#define twzcatch_all                                                                               \
	_hdl = 1;                                                                                      \
	break;                                                                                         \
	default:                                                                                       \
		_hdl = 1;

#define twztry_end                                                                                 \
	}                                                                                              \
	if(!_hdl && _pjb)                                                                              \
		longjmp(*_pjb, _fc);                                                                       \
	else if(!_hdl)                                                                                 \
		_twz_try_unhandled(twzcatch_fnum(), twzcatch_data());                                      \
	}
