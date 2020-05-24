#pragma once

#include <stdint.h>

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
 *  The enqueuer enqueues:
 *   T H
 * +--------------------+
 * |XX|  |  |  |  |  |  |
 * +--------------------+
 *  B
 *
 *  The enqueuer rings the bell
 *   T H
 * +--------------------+
 * |XX|  |  |  |  |  |  |
 * +--------------------+
 *     B (thread_sync)
 *
 * The dequeuer wakes up and gets the item
 *  XX
 *  |   HT
 * +|-------------------+
 * |^ |  |  |  |  |  |  |
 * +--------------------+
 *     B
 *
 * Finally, the dequeuer does thread_sync on the tail.
 *
 * Okay, but we can _also_ optimize these thread_syncs so that they only happen if someone is
 * waiting. We do this by stealing the upper bit of the bell and the tail. The enqueuer indicates
 * that it is waiting by setting the top bit of the bell (because it must write to it anyway, and
 * the dequeuer must read from it anyway), and
 * the dequeuer indicates it is waiting by setting the top bit of the tail (because it must write to
 * it anyway, and the enqueuer must read from it anyway).
 *
 * How does the dequeuer do the dequeue?
 * When it wakes up, it determines where the bell is and where the tail is. This tells it how many
 * items are waiting. Then, for each item in that range ( [tail, bell) ), it tries to dequeue them
 * (exchange the cmd_id with 0). It does this until there are no remaining items. If it gets back a
 * 0 for the cmd_id, that means that the enqueuer hasn't finished enqueing there.
 *
 * IF THIS IS A SINGLE PRODUCER QUEUE: this only happens if the dequeuer is speculating that there
 * may be items to dequeue. It can safely sleep on the bell.
 *
 * IF THIS IS A MULTIPLE PRODUCER QUEUE: Basically, this means that two threads got "slots"
 * and enqueued in different orders (which would be allowed), and then rang the bell before the
 * other could be ready. That is, A gets slot 0, B gets slot 1, B writes to slot 1 and incs the bell
 * and rings it. The consumer thread will not see anything in slot 0. What can it do? Well...
 * it could go back to sleep on the bell. That's because A will eventually get around to enqueuing
 * something there, inc the bell, and ring it.
 *
 * Note that this is leaving some work on the table. The consumer _has_ work to do (B's work), but
 * it's choosing not to do it until A manages to write its work. In practice, this is probably rare
 * (and we could choose to spin a bit on an item if we want).
 */

struct queue_entry {
	_Atomic uint32_t cmd_id; // some unique ID associated with this entry
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
		_Atomic uint32_t bell;
		_Atomic uint32_t waiters;
		uint8_t padb[64];
		_Atomic uint32_t tail;
	} subqueue[2];
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
