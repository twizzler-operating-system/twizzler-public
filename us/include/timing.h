struct timespec {
	uint64_t tv_sec;		/* seconds */
	long	 tv_nsec;	/* and nanoseconds */
};


static inline void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}
#define CLOCK_MONOTONIC 4
static inline int fbsd_clock_gettime(long id, struct timespec *p)
{
	return __syscall6(SYS_clock_gettime, id, (long)p, 0, 0, 0, 0);
}

