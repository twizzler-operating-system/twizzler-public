#include <twz/obj.h>
#include <twz/sys.h>

#include <errno.h>
#include <sys/time.h>
#include <time.h>

#include "syscalls.h"

/*  TODO: this should _probably_ be a kernel call, but w/e. Also, not portable. */
static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#include <twz/debug.h>
long linux_sys_clock_gettime(clockid_t clock, struct timespec *tp)
{
	static long tsc_ps = 0;
	if(!tsc_ps) {
		tsc_ps = sys_kconf(KCONF_ARCH_TSC_PSPERIOD, 0);
	}
	switch(clock) {
		uint64_t ts;
		case CLOCK_REALTIME:
		case CLOCK_REALTIME_COARSE:
		case CLOCK_PROCESS_CPUTIME_ID:
		/* TODO: these should probably be different */
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
		case CLOCK_MONOTONIC_COARSE:
			ts = rdtsc();
			/* TODO: overflow? */
			tp->tv_sec = ((long)((double)ts / (1000.0 / (double)tsc_ps))) / 1000000000ul;
			tp->tv_nsec = ((long)((double)ts / (1000.0 / (double)tsc_ps))) % 1000000000ul;
			break;
		default:
			twix_log(":: CGT: %ld\n", clock);
			return -ENOTSUP;
	}
	return 0;
}
