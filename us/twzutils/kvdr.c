#include "kv.h"
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

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

static inline unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

#define tc 1
int max = 1000000 / tc;
_Atomic long total_ops = 0;
_Atomic long total_ns = 0;
void test_insert(twzobj *index, twzobj *data, int id)
{
	int r;
	struct timespec start, end, diff;
	printf("Starting insert\n");
	clock_gettime(CLOCK_MONOTONIC, &start);
	for(uint32_t i = 0; i < max; i++) {
		uint64_t k = i | (long)id << 32;
		struct twzkv_item key = { .data = &k, .length = sizeof(k) };
		struct twzkv_item value = { .data = &k, .length = sizeof(k) };

		if((r = twzkv_put(&key, &value))) {
			printf("Error!\n");
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	timespec_diff(&start, &end, &diff);
	uint64_t d = diff.tv_nsec + diff.tv_sec * 1000000000ul;
	total_ops += max;
	total_ns += d;
	printf("INSERT: %ld ns :: %ld\n", d, d / max);
}

void test_lookup(twzobj *index, twzobj *data, int id)
{
	int r;
	struct timespec start, end, diff;
	clock_gettime(CLOCK_MONOTONIC, &start);
	for(uint32_t i = 0; i < max; i++) {
		uint64_t k = i | (long)id << 32;
		struct twzkv_item key = { .data = &k, .length = sizeof(k) };
		struct twzkv_item value;

		if((r = twzkv_get(&key, &value))) {
			printf("Error g!\n");
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	timespec_diff(&start, &end, &diff);
	uint64_t d = diff.tv_nsec + diff.tv_sec * 1000000000ul;
	total_ops += max;
	total_ns += d;
	printf("LOOKUP: %ld ns :: %ld\n", d, d / max);
}

void test(twzobj *index, twzobj *data)
{
	int max = 1000000;
	int r;
	struct timespec start, end, diff;
	for(int i = 0; i < 1000; i++) {
		clock_gettime(CLOCK_MONOTONIC, &start);
		clock_gettime(CLOCK_MONOTONIC, &end);
		timespec_diff(&start, &end, &diff);
		printf("CALIB: %ld ns :: %ld\n", diff.tv_nsec + diff.tv_sec * 1000000000ul, 0);
	}

	clock_gettime(CLOCK_MONOTONIC, &start);
	for(uint32_t i = 0; i < max; i++) {
		struct twzkv_item key = { .data = &i, .length = sizeof(i) };
		struct twzkv_item value = { .data = &i, .length = sizeof(i) };

		if((r = twzkv_put(&key, &value))) {
			printf("Error!\n");
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	timespec_diff(&start, &end, &diff);
	uint64_t d = diff.tv_nsec + diff.tv_sec * 1000000000ul;
	printf("INSERT: %ld ns :: %ld\n", d, d / max);

	clock_gettime(CLOCK_MONOTONIC, &start);
	for(uint32_t i = 0; i < max; i++) {
		struct twzkv_item key = { .data = &i, .length = sizeof(i) };
		struct twzkv_item value;

		if((r = twzkv_get(&key, &value))) {
			printf("Error g!\n");
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &end);
	timespec_diff(&start, &end, &diff);
	d = diff.tv_nsec + diff.tv_sec * 1000000000ul;
	printf("LOOKUP: %ld ns :: %ld\n", d, d / max);
}

void verify(twzobj *index, twzobj *data)
{
	struct twzkv_item key = { .data = "fruit", .length = 6 };
	struct twzkv_item val = { .data = "apple", .length = 6 };

	if(twzkv_put(&key, &val) == -1) {
		printf("ERROR put\n");
		return;
	}

	memset(&val, 0, sizeof(val));
	struct twzkv_item vret;

	if(twzkv_get(&key, &vret) == -1) {
		printf("ERROR get\n");
		return;
	}

	printf("Got: %s (%ld)\n", (char *)vret.data, vret.length);
}

int count = 30;
int main()
{
	setbuf(stdout, 0);
	printf("Hello, World! from nls!\n");

	twzobj indexo, datao;
	init_database(&indexo, &datao);

	// printf("VERIFY\n");
	// verify(&index, &data);

	// printf("TEST\n");

	test_insert(&indexo, &datao, 0);
	printf("INSERT Total Ops: %ld, Total ns: %ld\n", total_ops, total_ns);
	printf("       Latency per op: %ld; Throughput: %2.2f ops/s\n",
	  total_ns / total_ops,
	  ((float)total_ops * 1000000000) / total_ns);
	total_ops = total_ns = 0;
	test_lookup(&indexo, &datao, 0);
	printf("LOOKUP Total Ops: %ld, Total ns: %ld\n", total_ops, total_ns);
	printf("       Latency per op: %ld; Throughput: %2.2f ops/s\n",
	  total_ns / total_ops,
	  ((float)total_ops * 1000000000) / total_ns);
	total_ops = total_ns = 0;

	if(--count > 0)
		main();
	return 0;
}
