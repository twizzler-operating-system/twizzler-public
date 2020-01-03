#include <twz/obj.h>

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
	switch(clock) {
		uint64_t ts;
		case CLOCK_REALTIME:
		case CLOCK_REALTIME_COARSE:
		/* TODO: these should probably be different */
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
		case CLOCK_MONOTONIC_COARSE:
			ts = rdtsc();
			/* TODO: overflow? */
			tp->tv_sec = ((long)((double)ts / 2.32)) / 1000000000ul;
			tp->tv_nsec = ((long)((double)ts / 2.32)) % 1000000000ul;
			//		debug_printf(":: %ld -> %ld %ld\n", ts, tp->tv_sec, tp->tv_nsec);
			break;
		default:
			debug_printf(":: CGT: %ld\n", clock);
			return -ENOTSUP;
	}
	return 0;
}
