#pragma once

#include <stdatomic.h>
#include <stdint.h>
#include <string.h>

/* The queue is split into two regions after the header. The submission queue, which is required,
 * and the completion queue, which is optional. These are each referred to as "subqueues". A
 * subqueue operates as follows:
 *   - the head refers to the next element that can be filled.
 *   - the tail refers to the next element to be consumed.
 *   - the bell refers to 1+ the element before-which there are consumable items.
 *
 * Example, let's say the queue starts empty.
 *  HT
 * +--------------------+
 * |  |  |  |  |  |  |  |
 * +--------------------+
 *  B
 *
 *  The producer enqueues:
 *   T H
 * +--------------------+
 * |XX|  |  |  |  |  |  |
 * +--------------------+
 *  B
 *
 *  The producer rings the bell
 *   T H
 * +--------------------+
 * |XX|  |  |  |  |  |  |
 * +--------------------+
 *     B (thread_sync)
 *
 * The consumer wakes up and gets the item
 *  XX
 *  |   HT
 * +|-------------------+
 * |^ |  |  |  |  |  |  |
 * +--------------------+
 *     B
 *
 * Finally, the consumer does thread_sync on the tail.
 *
 * Okay, but we can _also_ optimize these thread_syncs so that they only happen if someone is
 * waiting. We do this by stealing the upper bit of the tail and using a wait count. Producers
 * indicate that they are waiting by incrementing the wait count (and decrementing it when they are
 * done waiting). The consumer indicates it's waiting by setting the top bit of the tail.
 *
 * How does the consumer do the dequeue?
 * When it wakes up, it determines where the bell is and where the tail is. This tells it how many
 * items are waiting. Then, for each item in that range ( [tail, bell) ), it checks the _turn_. The
 * turn (or phase) of the queue is a bit at the top of cmd_id that says which go-around of the queue
 * this entry came from. Specifically, the top bit is set to (1 - (head / length) % 2). That is, the
 * first time the producers enqueues to slot 0, it'll set the turn bit to 1. The next time (once it
 * wraps), it'll set the turn bit to 0. This alternating pattern lets the consumer tell if an entry
 * is new or old.
 *
 * So when the consumer comes around, it checks if the turn bit is on of off (depending on the
 * go-around of the queue its on).
 *
 * Note that this is needed for multiple producers. If there was a single producer this would not be
 * needed (knowing the bell position would be enough). But with MP there is a possible "race":
 *
 *   - A gets slot 0
 *   - B gets slot 1, and fills it.
 *   - B incs the bell
 *   - consumer wakes up, sees the queue isn't empty, but nothing is in slot 0 yet. It must wait for
 *   A to fill slot 0.
 *
 * This resolves, though, because A will to its own increment and wake operation on the bell, so
 * we'll have a chance to run again. Eventually, we'll get the items.
 *
 */

struct queue_entry {
	_Atomic uint32_t cmd_id; // top bit is turn bit.
	uint32_t info;           // some user-defined info
	char data[];
};

struct queue_hdr {
	uint64_t magic;
	uint64_t flags;
	struct {
		uint8_t pad[64];
		struct queue_entry *queue;
		size_t length;
		size_t stride;
		uint8_t pada[64];
		_Atomic uint32_t head;
		_Atomic uint32_t waiters; // how many producers are waiting
		_Atomic uint64_t bell;
		uint8_t padb[64];
		_Atomic uint64_t tail; // top bit indicates consumer is waiting.
	} subqueue[2];
	/* the kernel cannot use data after this point */
};

#define SUBQUEUE_SUBM 0
#define SUBQUEUE_CMPL 1

#define E_ISWAITING(q, i) ((q)->subqueue[i].waiters > 0)
#define D_ISWAITING(q, i) ((q)->subqueue[i].tail & (1ul << 31))

#define E_INCWAITING(q, i) ((q)->subqueue[i].waiters++)
#define D_SETWAITING(q, i) ((q)->subqueue[i].tail |= (1ul << 31))

#define E_DECWAITING(q, i) ((q)->subqueue[i].waiters--)
#define D_CLRWAITING(q, i) ((q)->subqueue[i].tail &= ~(1ul << 31))

#define is_full(head, tail, length) ({ (head & 0x7fffffff) - (tail & 0x7fffffff) >= length; })
#define is_empty(head, tail) ({ (head & 0x7fffffff) == (tail & 0x7fffffff); })

#if __KERNEL__

static inline int __wait_on(void *p, uint32_t v, int dq)
{
	return 0;
}

static inline int __wake_up(void *p, uint32_t v, int dq)
{
	return 0;
}

#else
#include <errno.h>
#include <twz/_sys.h>
#include <twz/thread.h>
static inline int __wait_on(void *p, uint32_t v, int dq)
{
	twz_thread_sync(THREAD_SYNC_SLEEP, p, v, NULL);
	/* TODO: errors */
	return 0;
}

static inline int __wake_up(void *p, uint32_t v, int dq)
{
	twz_thread_sync(THREAD_SYNC_WAKE, p, v, NULL);
	/* TODO: errors */
	return 0;
}
#endif

static inline _Bool is_turn(struct queue_hdr *hdr, int sq, uint32_t tail, struct queue_entry *entry)
{
	int turn = (tail / hdr->subqueue[sq].length) % 2;
	return (entry->cmd_id >> 31) != turn;
}

static inline struct queue_entry *__get_entry(struct queue_hdr *hdr, int sq, uint32_t pos)
{
	return (void *)((char *)hdr->subqueue[sq].queue
	                + ((pos % hdr->subqueue[sq].length) * hdr->subqueue[sq].stride));
}

static inline int queue_sub_enqueue(struct queue_hdr *hdr,
  int sq,
  struct queue_entry *submit,
  _Bool nonblock)
{
	int r;
	uint32_t h, t;

	/* indicate that we may be waiting. */
	E_INCWAITING(hdr, sq);

	/* part 1 -- reserve a slot. This is as easy as just incrementing the head atomically. */
	h = atomic_fetch_add(&hdr->subqueue[sq].head, 1);
	/* part 2 -- wait until there is space. If the queue is full, then sleep on the tail. We will be
	 * woken up by the consumer. */
	t = hdr->subqueue[sq].tail;
	while(is_full(h, t, hdr->subqueue[sq].length)) {
		if(nonblock) {
			return -EAGAIN;
		}
		if((r = __wait_on(&hdr->subqueue[sq].tail, t, 0)) < 0) {
			return r;
		}
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
	entry->info = submit->info;
	size_t datalen = hdr->subqueue[sq].stride - sizeof(struct queue_entry);
	memcpy(entry->data, submit->data, datalen);

	/* the turn alternates on passes through the queue */
	uint32_t turn = 1 - ((h / hdr->subqueue[sq].length) % 2);
	atomic_store(&entry->cmd_id, h | turn << 31);

	/* ring the bell! If the consumer isn't waiting, don't bother the kernel. */
	hdr->subqueue[sq].bell++;
	if(D_ISWAITING(hdr, sq)) {
		if((r = __wake_up(&hdr->subqueue[sq].bell, 1, 0)) < 0) {
			return r;
		}
	}
	E_DECWAITING(hdr, sq);
	return 0;
}

static inline int queue_sub_dequeue(struct queue_hdr *hdr,
  int sq,
  struct queue_entry *result,
  _Bool nonblock)
{
	int r;
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
		if(nonblock) {
			return -EAGAIN;
		}
		D_SETWAITING(hdr, sq);
		if((r = __wait_on(&hdr->subqueue[sq].bell, b, 1)) < 0) {
			return r;
		}
		/* we waited on the bell; read a new value */
		b = hdr->subqueue[sq].bell;
	}
	D_CLRWAITING(hdr, sq);

	/* the is_turn function does the acquire operation that pairs with the release on cmd_id in the
	 * enqueue function. */
	memcpy(result, entry, hdr->subqueue[sq].stride);

	/* update the tail, remembering not to overwrite the waiting bit */
	hdr->subqueue[sq].tail = (hdr->subqueue[sq].tail + 1) & 0x7fffffff;
	if(E_ISWAITING(hdr, sq)) {
		/* wake up anyone waiting on the queue being full. */
		if((r = __wake_up(&hdr->subqueue[sq].tail, 1, 1)) < 0) {
			return r;
		}
	}
	return 0;
}
