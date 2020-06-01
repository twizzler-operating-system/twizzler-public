#pragma once

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least32_t;
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

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

/* TODO: get rid of multiples and divides */

struct queue_entry {
	atomic_uint_least32_t cmd_id; // top bit is turn bit.
	uint32_t info;                // some user-defined info
#ifndef __cplusplus
	char data[];
#endif
};

struct queue_hdr {
	uint64_t magic;
	uint64_t flags;
	struct {
		uint8_t pad[64];
		struct queue_entry *queue;
		size_t l2length;
		size_t stride;
		uint8_t pada[64];
		atomic_uint_least32_t head;
		atomic_uint_least32_t waiters; // how many producers are waiting
		uint8_t padc[64];
		atomic_uint_least64_t bell;
		uint8_t padb[64];
		atomic_uint_least64_t tail; // top bit indicates consumer is waiting.
	} subqueue[2];
	/* the kernel cannot use data after this point */
};

#ifndef __QUEUE_TYPES_ONLY

#define SUBQUEUE_SUBM 0
#define SUBQUEUE_CMPL 1

#define E_ISWAITING(q, i) ((q)->subqueue[i].waiters > 0)
#define D_ISWAITING(q, i) ((q)->subqueue[i].tail & (1ul << 31))

#define E_INCWAITING(q, i) ((q)->subqueue[i].waiters++)
#define D_SETWAITING(q, i) ((q)->subqueue[i].tail |= (1ul << 31))

#define E_DECWAITING(q, i) ((q)->subqueue[i].waiters--)
#define D_CLRWAITING(q, i) ((q)->subqueue[i].tail &= ~(1ul << 31))

#define is_full(head, tail, l2length)                                                              \
	({ (head & 0x7fffffff) - (tail & 0x7fffffff) >= (1ul << l2length); })
#define is_empty(head, tail) ({ (head & 0x7fffffff) == (tail & 0x7fffffff); })

#if __KERNEL__

#include <object.h>
struct object;
int kernel_queue_wait_on(struct object *, void *, uint64_t);
int kernel_queue_wake_up(struct object *, void *, uint64_t);

static inline int __wait_on(struct object *obj, void *p, uint64_t v, int dq)
{
	(void)dq;
	return kernel_queue_wait_on(obj, p, v);
}

static inline int __wake_up(struct object *obj, void *p, uint64_t v, int dq)
{
	(void)dq;
	return kernel_queue_wake_up(obj, p, v);
}

static inline struct queue_entry *__get_entry(struct object *obj,
  struct queue_hdr *hdr,
  int sq,
  uint32_t pos)
{
	/* a queue object already has the kbase ref count > 1 */
	void *base = obj_get_kaddr(obj);
	void *r =
	  (void *)((char *)hdr->subqueue[sq].queue
	           + ((pos & ((1ul << hdr->subqueue[sq].l2length) - 1)) * hdr->subqueue[sq].stride));
	obj_release_kaddr(obj);
	// printk("[kq] get_entry : %p\n", (char *)base + (long)r);
	return (struct queue_entry *)((char *)base + (long)r);
}

#else
#include <errno.h>
#include <stdio.h>
#include <twz/_sys.h>
#include <twz/thread.h>
static inline int __wait_on(void *o, atomic_uint_least64_t *p, uint64_t v, int dq)
{
	(void)o;
	twz_thread_sync(THREAD_SYNC_SLEEP, p, v, NULL);
	/* TODO: errors */
	return 0;
}

static inline int __wake_up(void *o, atomic_uint_least64_t *p, uint64_t v, int dq)
{
	(void)o;
	// printf("wake: %d\n", dq);
	twz_thread_sync(THREAD_SYNC_WAKE, p, v, NULL);
	/* TODO: errors */
	return 0;
}

static inline struct queue_entry *__get_entry(
#if __KERNEL__
  struct object *obj,
#else
  twzobj *obj,
#endif
  struct queue_hdr *hdr,
  int sq,
  uint32_t pos)
{
	return (struct queue_entry *)((char *)twz_object_lea(obj, hdr->subqueue[sq].queue)
	                              + ((pos & ((1ul << hdr->subqueue[sq].l2length) - 1))
	                                 * hdr->subqueue[sq].stride));
}

#endif

static inline _Bool is_turn(struct queue_hdr *hdr, int sq, uint32_t tail, struct queue_entry *entry)
{
	uint32_t turn = (tail / (1ul << hdr->subqueue[sq].l2length)) % 2;
	return (entry->cmd_id >> 31) != turn;
}

static inline int queue_sub_enqueue(
#if __KERNEL__
  struct object *obj,
#else
  twzobj *obj,
#endif
  struct queue_hdr *hdr,
  int sq,
  struct queue_entry *submit,
  _Bool nonblock)
{
	int r;
	uint32_t h, t;

	/* indicate that we may be waiting. */

	/* part 1 -- reserve a slot. This is as easy as just incrementing the head atomically. */
	h = atomic_fetch_add(&hdr->subqueue[sq].head, 1);
	/* part 2 -- wait until there is space. If the queue is full, then sleep on the tail. We will be
	 * woken up by the consumer. */
	t = hdr->subqueue[sq].tail;
	int waiter = 0;
	if(is_full(h, t, hdr->subqueue[sq].l2length)) {
		waiter = 1;
		E_INCWAITING(hdr, sq);
		t = hdr->subqueue[sq].tail;
	}
	int attempts = 1000;
	while(is_full(h, t, hdr->subqueue[sq].l2length)) {
		if(nonblock) {
			if(waiter) {
				E_DECWAITING(hdr, sq);
			}
			return -EAGAIN;
		}
		if(attempts == 0) {
			if((r = __wait_on(obj, &hdr->subqueue[sq].tail, t, 0)) < 0) {
				E_DECWAITING(hdr, sq);
				return r;
			}
		} else {
			attempts--;
			asm volatile("pause");
		}
		/* grab the new tail value! */
		t = hdr->subqueue[sq].tail;
	}
	if(waiter) {
		E_DECWAITING(hdr, sq);
	}

	/* we use the top bit of cmd_id to indicate the turn of the queue, so make sure our slot doesn't
	 * set this bit */
	h &= 0x7fffffff;

	/* finally, write-in the data. We are using release-semantics for the store to cmd_id to ensure
	 * the non-atomically-written data put into the queue entry is visible to the consumer that does
	 * the paired acquire operation on the cmd_id. */
	struct queue_entry *entry = __get_entry(obj, hdr, sq, h);
	entry->info = submit->info;
	size_t datalen = hdr->subqueue[sq].stride - sizeof(struct queue_entry);
#ifdef __cplusplus
	memcpy((char *)entry + sizeof(*entry), (char *)submit + sizeof(*submit), datalen);
#else
	memcpy(entry->data, submit->data, datalen);
#endif

	/* the turn alternates on passes through the queue */
	uint32_t turn = 1 - ((h / (1ul << hdr->subqueue[sq].l2length)) % 2);
	atomic_store(&entry->cmd_id, h | (turn << 31));

	/* ring the bell! If the consumer isn't waiting, don't bother the kernel. */
	hdr->subqueue[sq].bell++;
	if(D_ISWAITING(hdr, sq)) {
		if((r = __wake_up(obj, &hdr->subqueue[sq].bell, 1, 0)) < 0) {
			return r;
		}
	}
	return 0;
}

static inline int queue_sub_dequeue(
#if __KERNEL__
  struct object *obj,
#else
  twzobj *obj,
#endif
  struct queue_hdr *hdr,
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
	struct queue_entry *entry = __get_entry(obj, hdr, sq, t);
	int attempts = 1000;
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
		if(attempts == 0) {
			D_SETWAITING(hdr, sq);
			if(is_empty(b, t) || !is_turn(hdr, sq, t, entry)) {
				if((r = __wait_on(obj, &hdr->subqueue[sq].bell, b, 1)) < 0) {
					D_CLRWAITING(hdr, sq);
					return r;
				}
			}
		} else {
			attempts--;
			asm volatile("pause");
		}
		/* we waited on the bell; read a new value */
		b = hdr->subqueue[sq].bell;
	}
	if(attempts == 0) {
		D_CLRWAITING(hdr, sq);
	}

	/* the is_turn function does the acquire operation that pairs with the release on cmd_id in
	 * the enqueue function. */
	memcpy(result, entry, hdr->subqueue[sq].stride);

	/* update the tail, remembering not to overwrite the waiting bit */
	hdr->subqueue[sq].tail = (hdr->subqueue[sq].tail + 1) & 0x7fffffff;
	if(E_ISWAITING(hdr, sq)) {
		/* wake up anyone waiting on the queue being full. */
		if((r = __wake_up(obj, &hdr->subqueue[sq].tail, 1, 1)) < 0) {
			return r;
		}
	}
	return 0;
}

#ifndef __KERNEL__
struct queue_dequeue_multiple_spec {
	twzobj *obj;
	struct queue_entry *result;
	int sq;
	int ret;
};

static inline ssize_t queue_sub_dequeue_multiple(size_t count,
  struct queue_dequeue_multiple_spec *specs)
{
	if(count > 128)
		return -EINVAL;
	size_t sleep_count = 0;
	struct sys_thread_sync_args tsa[count];
	for(size_t i = 0; i < count; i++) {
		struct queue_hdr *hdr = (struct queue_hdr *)twz_object_base(specs[i].obj);
		uint32_t t, b;
		/* grab the tail. Remember, we use the top bit to indicate we are waiting. */
		t = hdr->subqueue[specs[i].sq].tail & 0x7fffffff;
		b = hdr->subqueue[specs[i].sq].bell;

		/* we will be dequeuing at t, so just get the entry. But we might have to wait for it!
		 */
		struct queue_entry *entry = __get_entry(specs[i].obj, hdr, specs[i].sq, t);

		if(is_empty(b, t) || !is_turn(hdr, specs[i].sq, t, entry)) {
			specs[i].ret = 0;
			twz_thread_sync_init(
			  &tsa[sleep_count++], THREAD_SYNC_SLEEP, &hdr->subqueue[specs[i].sq].bell, b, NULL);
		} else {
			queue_sub_dequeue(specs[i].obj, hdr, specs[i].sq, specs[i].result, true);
			specs[i].ret = 1;
		}
	}

	if(sleep_count == count) {
		int r = twz_thread_sync_multiple(sleep_count, tsa);
		/* TODO: errors */
		return queue_sub_dequeue_multiple(count, specs);
	}

	return count - sleep_count;
}
#endif

#endif
