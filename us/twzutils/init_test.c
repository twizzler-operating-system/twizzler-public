//#define __HAVE_CLWB 1

#include <twz/obj.h>
#include <twz/tx.h>
static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#define MANUAL 1

#include <twz/tx.h>
struct foo {
#if MANUAL
	int ox, oy;
	int valid;
#endif
	int x, y;
	struct twz_tx tx;
};

twzobj o;
void init_test_init(void)
{
	if((twz_object_new(&o, NULL, NULL, TWZ_OC_DFL_WRITE | TWZ_OC_DFL_READ))) {
		abort();
	}
	struct foo *f = twz_object_base(&o);
	f->tx.logsz = 1024;
	f->x = f->y = 0;
}

void init_test_iter(void)
{
	struct foo *f = twz_object_base(&o);
#if !MANUAL
	// 2979 (emu;clflush)
	// 2472 (rel;clflush)
	// 2527 (emu;clfopt)
	// 2101 (rel;clfopt)
	TXSTART(&o, &f->tx)
	{
		TXRECORD(&f->tx, &f->x);
		TXRECORD(&f->tx, &f->y);
		f->x = 5;
		f->y = 6;
		// TXABORT(<errcode>);
		TXCOMMIT;
	}
	TXEND;
#else
	// just writing values
	// 519 (emu;clflush)
	// 524 (emu;clfopt)
	// 456 (rel;clflush)
	// 375 (rel;clfopt)
	// manual safety
	// 1412 (rel;clfopt)
	// 1841 (emu;clfopt)
	f->valid = 0;
	_clwb(&f->valid);
	_pfence();
	f->ox = f->x;
	f->oy = f->y;
	_clwb(&f->ox);
	_clwb(&f->oy);
	_pfence();
	f->x = 5;
	f->y = 6;
	_clwb(&f->x);
	_clwb(&f->y);
	_pfence();
	f->valid = 1;
	_clwb(&f->valid);
	_pfence();
#endif
}
