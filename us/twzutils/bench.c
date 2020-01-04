#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <twz/debug.h>
#include <twz/obj.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
	return;
}

void do_test(char *mem, long stride, int w)
{
	for(long i = 0; i < 1000000000ul; i++) {
		//	if(i % 10000000 == 0)
		//		debug_printf(":: %ld / 1000000000\n", i);
		char *x = mem + (i * stride) % (1024 * 1024 * 1024 - OBJ_NULLPAGE_SIZE);
		char r = 0;
		if(w)
			*x = stride;
		else
			r = *x;
		asm volatile("" ::"m"(x), "r"(r) : "memory");
	}
}

int main()
{
	twzobj obj;
	if(twz_object_new(&obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE) < 0)
		abort();

	char *mem = twz_object_base(&obj);

	struct timespec st, en, df;

	for(int i = 0; i < 24; i++) {
		clock_gettime(CLOCK_MONOTONIC, &st);
		do_test(mem, 1ul << (i / 4 + 1), (i / 2) % 2);
		clock_gettime(CLOCK_MONOTONIC, &en);
		timespec_diff(&st, &en, &df);
		printf("stride %ld, w=%d: %lf seconds\n",
		  1ul << (i / 4 + 1),
		  (i / 2) % 2,
		  df.tv_sec + (double)df.tv_nsec / 1000000000.0);
	}
}
