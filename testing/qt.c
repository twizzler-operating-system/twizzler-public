#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>

//#define PR printf
#define PR(...)

struct queue_hdr *create_q(size_t len)
{
	struct queue_hdr *hdr = malloc(sizeof(*hdr));
	memset(hdr, 0, sizeof(*hdr));
	hdr->subqueue[0].length = len;
	hdr->subqueue[1].length = len;
	hdr->subqueue[0].stride = 8;
	hdr->subqueue[1].stride = 8;
	hdr->subqueue[0].queue = calloc(len, 8);
	hdr->subqueue[1].queue = calloc(len, 8);

	return hdr;
}

#define QUEUE_NONBLOCK 1
int queue_submit(struct queue_hdr *hdr, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(hdr, SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_complete(struct queue_hdr *hdr, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(hdr, SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

int N = 3;
_Atomic int done = 0;
int icount = 100000;
void *prod(void *a)
{
	static _Atomic int __id = 0;

	int id = ++__id;
	struct queue_hdr *hdr = a;
	unsigned x = 0;
	for(x = 0; x < icount; x++) {
		PR("%d: enqueue %d\n", id, x);
		//	for(volatile long i = 0; i < 4000; i++)
		//		;
		//	usleep(1);
		struct queue_entry e;
		e.info = x * N + (id - 1);
		if(queue_sub_enqueue(hdr, SUBQUEUE_SUBM, &e, 0) < 0)
			abort();
	}
	done++;
	return NULL;
}

size_t nrb = 0x100000;
char *bm;
unsigned max = 0;
void *cons(void *a)
{
	struct queue_hdr *hdr = a;
	while(1) {
		struct queue_entry e;
		if(queue_sub_dequeue(hdr, SUBQUEUE_SUBM, &e, done == N) < 0) {
			if(done == N)
				break;
			abort();
		}
		if(done == N && e.info == 0)
			break;
		if(e.info > max)
			max = e.info;
		if(e.info >= nrb)
			abort();
		bm[e.info / 8] |= (1 << (e.info % 8));
		PR("                                         GOT %d\n", e.info);
	}
}

#include <time.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000ul;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}

	return;
}

int main()
{
	struct queue_hdr *hdr = create_q(32);
	bm = malloc(nrb / 8);
	memset(bm, 0, nrb / 8);

	struct timespec start, end, diff;
	clock_gettime(CLOCK_MONOTONIC, &start);
	pthread_t threads[N];
	for(int i = 0; i < N; i++) {
		pthread_create(&threads[i], NULL, prod, hdr);
	}

	cons(hdr);

	for(int i = 0; i < N; i++) {
		pthread_join(threads[i], NULL);
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	timespec_diff(&start, &end, &diff);

	uint64_t ns = diff.tv_nsec + diff.tv_sec * 1000000000ul;
	printf("time %d threads %d max %d inserted/th %ld ns ( %4.4lf seconds )\n",
	  N,
	  max + 1,
	  icount,
	  ns,
	  (double)ns / 1000000000);
	printf("futex-enqueue-total: %9ld\n", calls_to_futex);
	printf("futex-enqueue-sleep: %9ld\n", ctf_sleeps);
	printf("futex-enqueue-wakes: %9ld\n", ctf_wakes);
	printf("futex-dequeue-total: %9ld\n", dqcalls_to_futex);
	printf("futex-dequeue-sleep: %9ld\n", dqctf_sleeps);
	printf("futex-dequeue-wakes: %9ld\n", dqctf_wakes);

	int ret = 0;
	fprintf(stderr, "DONE :: REPORT\n");
	for(unsigned i = 0; i <= max; i++) {
		if((bm[i / 8] & (1 << (i % 8))) == 0) {
			fprintf(stderr, "missing entry %d\n", i);
			ret = 1;
		}
	}

	return ret;
}
