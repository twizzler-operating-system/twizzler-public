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
		_Atomic uint32_t bell;
		_Atomic uint32_t waiters; // how many producers are waiting
		uint8_t padb[64];
		_Atomic uint32_t tail; // top bit indicates consumer is waiting.
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
