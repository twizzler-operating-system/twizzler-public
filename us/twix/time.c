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

long linux_sys_clock_gettime(clockid_t clock, struct timespec *tp)
{
	switch(clock) {
		uint64_t ts;
		case CLOCK_MONOTONIC:
		case CLOCK_MONOTONIC_RAW:
		case CLOCK_MONOTONIC_COARSE:
			ts = rdtsc();
			/* TODO: overflow? */
			tp->tv_sec = (ts / 4) / 1000000000ul;
			tp->tv_nsec = (ts / 4) % 1000000000ul;
			break;
		default:
			return -ENOTSUP;
	}
	return 0;
}
