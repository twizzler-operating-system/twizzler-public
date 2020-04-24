#include "syscalls.h"
#include <stdint.h>
#include <string.h>

#define RDRAND_RETRIES 10

#if 0
static int rdrand16_step(uint16_t *rand)
{
	unsigned char ok;

	asm volatile("rdrand %0; setc %1" : "=r"(*rand), "=qm"(ok));

	return (int)ok;
}

static int rdrand32_step(uint32_t *rand)
{
	unsigned char ok;

	asm volatile("rdrand %0; setc %1" : "=r"(*rand), "=qm"(ok));

	return (int)ok;
}
#endif
static int rdrand64_step(uint64_t *rand)
{
	unsigned char ok;

	asm volatile("rdrand %0; setc %1" : "=r"(*rand), "=qm"(ok));

	return (int)ok;
}
#if 0
static int rdrand16_retry(unsigned int retries, uint16_t *rand)
{
	unsigned int count = 0;

	while(count <= retries) {
		if(rdrand16_step(rand)) {
			return 1;
		}

		++count;
	}

	return 0;
}

static int rdrand32_retry(unsigned int retries, uint32_t *rand)
{
	unsigned int count = 0;

	while(count <= retries) {
		if(rdrand32_step(rand)) {
			return 1;
		}

		++count;
	}

	return 0;
}
#endif
static int rdrand64_retry(unsigned int retries, uint64_t *rand)
{
	unsigned int count = 0;

	while(count <= retries) {
		if(rdrand64_step(rand)) {
			return 1;
		}

		++count;
	}

	return 0;
}

#if 0
/* Get n 32-bit uints using 64-bit rands */

static unsigned int rdrand_get_n_uints(unsigned int n, unsigned int *dest)
{
	unsigned int i;
	uint64_t *qptr = (uint64_t *)dest;
	unsigned int total_uints = 0;
	unsigned int qwords = n / 2;

	for(i = 0; i < qwords; ++i, ++qptr) {
		if(rdrand64_retry(RDRAND_RETRIES, qptr)) {
			total_uints += 2;
		} else {
			return total_uints;
		}
	}

	/* Fill the residual */

	if(n % 2) {
		unsigned int *uptr = (unsigned int *)qptr;

		if(rdrand32_step(uptr)) {
			++total_uints;
		}
	}

	return total_uints;
}
#endif

static unsigned int rdrand_get_bytes(unsigned int n, unsigned char *dest)
{
	unsigned char *headstart, *tailstart;
	uint64_t *blockstart;
	unsigned int count, ltail, lhead, lblock;
	uint64_t i, temprand;

	/* Get the address of the first 64-bit aligned block in the
	 * destination buffer. */

	headstart = dest;
	if(((uint64_t)headstart % (uint64_t)8) == 0) {
		blockstart = (uint64_t *)headstart;
		lblock = n;
		lhead = 0;
	} else {
		blockstart = (uint64_t *)(((uint64_t)headstart & ~(uint64_t)7) + (uint64_t)8);

		lblock = n - (8 - (unsigned int)((uint64_t)headstart & (uint64_t)8));

		lhead = (unsigned int)((uint64_t)blockstart - (uint64_t)headstart);
	}

	/* Compute the number of 64-bit blocks and the remaining number
	 * of bytes (the tail) */

	ltail = n - lblock - lhead;
	count = lblock / 8; /* The number 64-bit rands needed */

	if(ltail) {
		tailstart = (unsigned char *)((uint64_t)blockstart + (uint64_t)lblock);
	}

	/* Populate the starting, mis-aligned section (the head) */

	if(lhead) {
		if(!rdrand64_retry(RDRAND_RETRIES, &temprand)) {
			return 0;
		}

		memcpy(headstart, &temprand, lhead);
	}

	/* Populate the central, aligned block */

	for(i = 0; i < count; ++i, ++blockstart) {
		if(!rdrand64_retry(RDRAND_RETRIES, blockstart)) {
			return i * 8 + lhead;
		}
	}

	/* Populate the tail */

	if(ltail) {
		if(!rdrand64_retry(RDRAND_RETRIES, &temprand)) {
			return count * 8 + lhead;
		}

		memcpy(tailstart, &temprand, ltail);
	}

	return n;
}

long linux_sys_getrandom(char *buf, size_t len, unsigned int flags)
{
	return rdrand_get_bytes(len, buf);
}
