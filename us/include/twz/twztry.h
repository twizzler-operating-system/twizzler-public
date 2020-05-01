#pragma once

#ifdef __cplusplus
#error "this header is only available for C code; use C++ exceptions in C++ code"
#endif

/* this header exposes a try-catch construction for C code, useful for Twizzler code that needs to
 * translate or dereference pointers while allowing for some kinds of failure. Here is generally how
 * to use them:
 *
 * twztry {
 *     some_operation();
 * } twzcatch(FAULT_NULL) {
 *     printf("caught fault %d, with data %p\n", twzcatch_fnum(), twzcatch_data());
 * } twzcatch_all {
 *     printf("caught some other exception\n");
 * }
 *
 * The twztry macro starts a try-catch block, inside which any faults that aren't already handled
 * are treated as exceptions. This includes faults raised by functions like twz_object_lea. Lines
 * specifying certain faults that this block can handle (twzcatch(FAULT_NULL)) must specify an
 * actual fault (see twz/fault.h). The accessor functions twzcatch_fnum() and twzcatch_data() can be
 * used to get access to context for the fault. The fnum is the fault number, and the data is the
 * fault_ struct corresponding to the fault (see twz/fault.h).
 *
 * The macro twzcatch_all can be used to catch all faults not explicitly caught.
 */

#include <setjmp.h>

/* these should not be accessed directly by user code, use the macros below. */
extern _Thread_local jmp_buf *_Atomic _twz_jmp_buf;
extern _Thread_local void *_Atomic _twz_excep_data;

/* DO NOT call this directly; it's used in the try-catch macros below and should not be called by
 * user code */
extern void _twz_try_unhandled(int, void *);

/* start a try-catch block. Use our stack to backup the value of _twz_jmp_buf. */
#define twztry                                                                                     \
	{                                                                                              \
		jmp_buf _jb;                                                                               \
		jmp_buf *_pjb = _twz_jmp_buf;                                                              \
		_twz_jmp_buf = &_jb;                                                                       \
		int _fc, _hdl = 0;                                                                         \
		switch((_fc = setjmp(_jb))) {                                                              \
			case 0:

/* catch fault number f. Note that because fault numbers start at 0, we need to add 1 to them
 * because setjmp returns 0 on first pass. The corresponding code in libtwz must take this into
 * account too. */
#define twzcatch(f)                                                                                \
	_hdl = 1;                                                                                      \
	break;                                                                                         \
	case((f) + 1):                                                                                 \
		_hdl = 1;

/* access the data and the fault number for the fault that has just been caught */
#define twzcatch_data() _twz_excep_data
#define twzcatch_fnum() ({ _fc - 1; })

/* catch all faults not caught by explicit catches */
#define twzcatch_all                                                                               \
	_hdl = 1;                                                                                      \
	break;                                                                                         \
	default:                                                                                       \
		_hdl = 1;

/* end the try-catch block. Propagates faults that have not been caught */
#define twztry_end                                                                                 \
	}                                                                                              \
	_twz_jmp_buf = _pjb;                                                                           \
	if(!_hdl && _pjb)                                                                              \
		longjmp(*_pjb, _fc);                                                                       \
	else if(!_hdl)                                                                                 \
		_twz_try_unhandled(twzcatch_fnum(), twzcatch_data());                                      \
	}

/* explicitly propagate faults up the stack. If there is no parent try-catch block, die */
#define twzcatch_propagate(f, d)                                                                   \
	_twz_excep_data = d;                                                                           \
	if(_pjb)                                                                                       \
		longjmp(*_pjb, f);                                                                         \
	else                                                                                           \
		_twz_try_unhandled(f, d);                                                                  \
	\
