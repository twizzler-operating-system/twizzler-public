#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "queue.h"

#include <linux/futex.h>
#include <stdatomic.h>
#include <sys/syscall.h>
#include <unistd.h>

_Atomic size_t calls_to_futex = 0;
_Atomic size_t ctf_wakes = 0, ctf_sleeps = 0;
_Atomic size_t dqcalls_to_futex = 0;
_Atomic size_t dqctf_wakes = 0, dqctf_sleeps = 0;
int futex(void *uaddr,
  int futex_op,
  int val,
  const struct timespec *timeout,
  int *uaddr2,
  int val3,
  int dq)
{
	if(dq) {
		dqcalls_to_futex++;
		if(futex_op == FUTEX_WAIT)
			dqctf_sleeps++;
		else
			dqctf_wakes++;
	} else {
		calls_to_futex++;
		if(futex_op == FUTEX_WAIT)
			ctf_sleeps++;
		else
			ctf_wakes++;
	}
	return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

static inline int __wait_on(void *p, uint32_t v, int dq)
{
	return futex(p, FUTEX_WAIT, v, NULL, NULL, 0, dq);
}

static inline int __wake_up(void *p, uint32_t v, int dq)
{
	return futex(p, FUTEX_WAKE, v, NULL, NULL, 0, dq);
}

struct queue_hdr *create_q(size_t len)
{
	struct queue_hdr *hdr = malloc(sizeof(*hdr));
	memset(hdr, 0, sizeof(*hdr));
	hdr->subqueue[0].length = len;
	hdr->subqueue[1].length = len;
	hdr->subqueue[0].stride = len;
	hdr->subqueue[1].stride = len;
	hdr->subqueue[0].queue = calloc(len, 8);
	hdr->subqueue[1].queue = calloc(len, 8);

	return hdr;
}

//#define PR printf
#define PR(...)

static inline struct queue_entry *__get_entry(struct queue_hdr *hdr, int sq, uint32_t pos)
{
	return (void *)((char *)hdr->subqueue[sq].queue
	                + ((pos % hdr->subqueue[sq].length) * hdr->subqueue[sq].stride));
}

void queue_sub_enqueue(struct queue_hdr *hdr, int id, int sq, uint32_t data)
{
	uint32_t h, t;

	/* indicate that we may be waiting. */
	E_INCWAITING(hdr, sq);

	/* part 1 -- reserve a slot. This is as easy as just incrementing the head atomically. */
	h = atomic_fetch_add(&hdr->subqueue[sq].head, 1);
	/* part 2 -- wait until there is space. If the queue is full, then sleep on the tail. We will be
	 * woken up by the consumer. */
	t = hdr->subqueue[sq].tail;
	while(is_full(h, t, hdr->subqueue[sq].length)) {
		__wait_on(&hdr->subqueue[sq].tail, t, 0);
		/* grab the new tail value! */
		t = hdr->subqueue[sq].tail;
	}

	/* we use the top bit of cmd_id to indicate the turn of the queue, so make sure our slot doesn't
	 * set this bit */
	h &= 0x7fffffff;

	/* finally, write-in the data. We are using release-semantics for the store to cmd_id to ensure
	 * the non-atomically-written data put into the queue entry is visible to the consumer that does
	 * the paired acquire operation on the cmd_id. */
	struct queue_entry *entry = __get_entry(hdr, sq, h);
	entry->info = data;
	/* the turn alternates on passes through the queue */
	uint32_t turn = 1 - ((h / hdr->subqueue[sq].length) % 2);
	atomic_store(&entry->cmd_id, h | turn << 31);

	/* ring the bell! If the consumer isn't waiting, don't bother the kernel. */
	hdr->subqueue[sq].bell++;
	if(D_ISWAITING(hdr, sq)) {
		__wake_up(&hdr->subqueue[sq].bell, 1, 0);
	}
	E_DECWAITING(hdr, sq);
}

static inline _Bool is_turn(struct queue_hdr *hdr, int sq, uint32_t tail, struct queue_entry *entry)
{
	int turn = (tail / hdr->subqueue[sq].length) % 2;
	return (entry->cmd_id >> 31) != turn;
}

uint32_t queue_sub_dequeue(struct queue_hdr *hdr, int sq, _Bool nonblock)
{
	uint32_t t, b;
	/* grab the tail. Remember, we use the top bit to indicate we are waiting. */
	t = hdr->subqueue[sq].tail & 0x7fffffff;
	b = hdr->subqueue[sq].bell;

	/* we will be dequeuing at t, so just get the entry. But we might have to wait for it! */
	struct queue_entry *entry = __get_entry(hdr, sq, t);
	while(is_empty(b, t) || !is_turn(hdr, sq, t, entry)) {
		/* sleep if the queue is empty (in which case don't bother checking the turn) OR if the
		 * entry's turn is wrong. This happens if multiple enqueuers have raced as follows:
		 *   - A: get slot 0
		 *   - B: get slot 1
		 *   - B: fill slot 1
		 *   - B: move bell to 1 (indicates slot 0 is ready)
		 *   - consumer runs and sees bell at 1 (queue not empty), but nothing in slot 0.
		 *
		 *   When A gets scheduled, this will resolve as it will move the bell. */
		if(nonblock)
			return 0;
		D_SETWAITING(hdr, sq);
		__wait_on(&hdr->subqueue[sq].bell, b, 1);
		/* we waited on the bell; read a new value */
		b = hdr->subqueue[sq].bell;
	}
	D_CLRWAITING(hdr, sq);

	/* the is_turn function does the acquire operation that pairs with the release on cmd_id in the
	 * enqueue function. */
	uint32_t data = entry->info;

	/* update the tail, remembering not to overwrite the waiting bit */
	hdr->subqueue[sq].tail = (hdr->subqueue[sq].tail + 1) & 0x7fffffff;
	if(E_ISWAITING(hdr, sq)) {
		/* wake up anyone waiting on the queue being full. */
		__wake_up(&hdr->subqueue[sq].tail, 1, 1);
	}
	return data;
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
		enqueue(hdr, id, SUBQUEUE_SUBM, x * N + (id - 1));
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
		unsigned y = dequeue(hdr, SUBQUEUE_SUBM, done == N);
		if(done == N && y == 0)
			break;
		if(y > max)
			max = y;
		if(y >= nrb)
			abort();
		bm[y / 8] |= (1 << (y % 8));
		PR("                                         GOT %d\n", y);
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
