//#define __HAVE_CLWB 1

#include <stdio.h>
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
	int ox, oy;
	char pad0[64];
	int valid;
	char pad1[64];
	int x, y;
	char pad2[64];
	struct twz_tx tx;
};

#include <twz/debug.h>
#include <twz/sys.h>
twzobj o;

/*ITER
clwb 0x80000001088 (0x2000000042)
fence
clwb 0x80000001048 (0x2000000041)
fence
clwb 0x80000001000 (0x2000000040)
clwb 0x80000001004 (0x2000000040)
fence
clwb 0x80000001048 (0x2000000041)
fence
*/

/* ITER
clwb 0x80000001000 (0x2000000040)
fence
clwb 0x80000001048 (0x2000000041)
fence
clwb 0x8000000108C (0x2000000042)
fence
clwb 0x80000001048 (0x2000000041)
fence
*/

void init_test_init(void)
{
	if((twz_object_new(&o, NULL, NULL, TWZ_OC_DFL_WRITE | TWZ_OC_DFL_READ))) {
		abort();
	}
	struct foo *f = twz_object_base(&o);
	f->tx.logsz = 1024;
	f->x = f->y = 0;

	long r = sys_kconf(KCONF_RDRESET, 0);
	fprintf(stderr, "RDRESET: %ld\n", r);

#if 0
	int rcode;
	TXOPT_START(&o, &f->tx, rcode)
	{
		TXOPT_RECORD(&f->tx, &f->x);
		TXOPT_RECORD(&f->tx, &f->y);
		f->x = 2;
		f->y = 3;
		TXOPT_COMMIT;
		// TXOPT_ABORT(123);
	}
	TXOPT_END;


	for(;;)
		;
#endif
}

void init_test_iter(void)
{
	struct foo *f = twz_object_base(&o);
#if !MANUAL
#if 1
	// 2979 (emu;clflush)
	// 2472 (rel;clflush)
	// 2527 (emu;clfopt)
	// 2101 (rel;clfopt)
	TXSTART(&o, &f->tx)
	{
		//	TXRECORD(&f->tx, &f->x);
		//	TXRECORD(&f->tx, &f->y);
		TXRECORD_TMP(&f->tx, &f->x);
		TXRECORD_TMP(&f->tx, &f->y);
		TX_RECORD_COMMIT(&f->tx);
		f->x = 5;
		f->y = 6;
		// TXABORT(<errcode>);
		TXCOMMIT;
	}
	TXEND;
#else
	int rcode;
	TXOPT_START(&o, &f->tx, rcode)
	{
		TXOPT_RECORD(&f->tx, &f->x);
		TXOPT_RECORD(&f->tx, &f->y);
		f->x = 5;
		f->y = 6;
		// TXABORT(<errcode>);
		TXOPT_COMMIT;
	}
	TXOPT_END;

#endif
#else
	// just writing values
	// 519 (emu;clflush)
	// 524 (emu;clfopt)
	// 456 (rel;clflush)
	// 375 (rel;clfopt)
	// manual safety
	// 1377 (rel;clfopt)
	// 1668 (emu;clfopt)
	f->ox = f->x;
	f->oy = f->y;
	_clwb(&f->ox);
	//_clwb(&f->oy);
	_pfence();
	f->valid = 0;
	_clwb(&f->valid);
	_pfence();

	f->x = 5;
	f->y = 6;
	_clwb(&f->x);
	//_clwb(&f->y);
	_pfence();
	f->valid = 1;
	_clwb(&f->valid);
	//_pfence();
#endif
}
