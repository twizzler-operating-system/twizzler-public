#include <rand.h>
#include <spinlock.h>
#include <string.h>

typedef unsigned long long ub8;
#define UB8MAXVAL 0xffffffffffffffffLL
#define UB8BITS 64
typedef signed long long sb8;
#define SB8MAXVAL 0x7fffffffffffffffLL
typedef unsigned long int ub4; /* unsigned 4-byte quantities */
#define UB4MAXVAL 0xffffffff
typedef signed long int sb4;
#define UB4BITS 32
#define SB4MAXVAL 0x7fffffff
typedef unsigned short int ub2;
#define UB2MAXVAL 0xffff
#define UB2BITS 16
typedef signed short int sb2;
#define SB2MAXVAL 0x7fff
typedef unsigned char ub1;
#define UB1MAXVAL 0xff
#define UB1BITS 8
typedef signed char sb1; /* signed 1-byte quantities */
#define SB1MAXVAL 0x7f
typedef int word; /* fastest type available */

#define bis(target, mask) ((target) |= (mask))
#define bic(target, mask) ((target) &= ~(mask))
#define bit(target, mask) ((target) & (mask))
#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif /* min */
#ifndef max
#define max(a, b) (((a) < (b)) ? (b) : (a))
#endif /* max */
#ifndef align
#define align(a) (((ub4)a + (sizeof(void *) - 1)) & (~(sizeof(void *) - 1)))
#endif /* align */
#ifndef abs
#define abs(a) (((a) > 0) ? (a) : -(a))
#endif
#define TRUE 1
#define FALSE 0
#define SUCCESS 0 /* 1 on VAX */

ub8 randrsl[RANDSIZ], randcnt;

/*
------------------------------------------------------------------------------
 If (flag==TRUE), then use the contents of randrsl[0..255] as the seed.
------------------------------------------------------------------------------
*/
static void randinit(/*_ word flag _*/);

static void isaac64();

/*
------------------------------------------------------------------------------
 Call rand() to retrieve a single 64-bit random value
------------------------------------------------------------------------------
*/
#define rand()                                                                                     \
	(!randcnt-- ? (isaac64(), randcnt = RANDSIZ - 1, randrsl[randcnt]) : randrsl[randcnt])

extern ub8 randrsl[RANDSIZ], randcnt;
static ub8 mm[RANDSIZ];
static ub8 aa = 0, bb = 0, cc = 0;

#define ind(mm, x) (*(ub8 *)((ub1 *)(mm) + ((x) & ((RANDSIZ - 1) << 3))))
#define rngstep(mix, a, b, mm, m, m2, r, x)                                                        \
	{                                                                                              \
		x = *m;                                                                                    \
		a = (mix) + *(m2++);                                                                       \
		*(m++) = y = ind(mm, x) + a + b;                                                           \
		*(r++) = b = ind(mm, y >> RANDSIZL) + x;                                                   \
	}

static void isaac64()
{
	register ub8 a, b, x, y, *m, *m2, *r, *mend;
	m = mm;
	r = randrsl;
	a = aa;
	b = bb + (++cc);
	for(m = mm, mend = m2 = m + (RANDSIZ / 2); m < mend;) {
		rngstep(~(a ^ (a << 21)), a, b, mm, m, m2, r, x);
		rngstep(a ^ (a >> 5), a, b, mm, m, m2, r, x);
		rngstep(a ^ (a << 12), a, b, mm, m, m2, r, x);
		rngstep(a ^ (a >> 33), a, b, mm, m, m2, r, x);
	}
	for(m2 = mm; m2 < mend;) {
		rngstep(~(a ^ (a << 21)), a, b, mm, m, m2, r, x);
		rngstep(a ^ (a >> 5), a, b, mm, m, m2, r, x);
		rngstep(a ^ (a << 12), a, b, mm, m, m2, r, x);
		rngstep(a ^ (a >> 33), a, b, mm, m, m2, r, x);
	}
	bb = b;
	aa = a;
}

#define mix(a, b, c, d, e, f, g, h)                                                                \
	{                                                                                              \
		a -= e;                                                                                    \
		f ^= h >> 9;                                                                               \
		h += a;                                                                                    \
		b -= f;                                                                                    \
		g ^= a << 9;                                                                               \
		a += b;                                                                                    \
		c -= g;                                                                                    \
		h ^= b >> 23;                                                                              \
		b += c;                                                                                    \
		d -= h;                                                                                    \
		a ^= c << 15;                                                                              \
		c += d;                                                                                    \
		e -= a;                                                                                    \
		b ^= d >> 14;                                                                              \
		d += e;                                                                                    \
		f -= b;                                                                                    \
		c ^= e << 20;                                                                              \
		e += f;                                                                                    \
		g -= c;                                                                                    \
		d ^= f >> 17;                                                                              \
		f += g;                                                                                    \
		h -= d;                                                                                    \
		e ^= g << 14;                                                                              \
		g += h;                                                                                    \
	}

static void randinit(flag) word flag;
{
	word i;
	ub8 a, b, c, d, e, f, g, h;
	aa = bb = cc = (ub8)0;
	a = b = c = d = e = f = g = h = 0x9e3779b97f4a7c13LL; /* the golden ratio */

	for(i = 0; i < 4; ++i) /* scramble it */
	{
		mix(a, b, c, d, e, f, g, h);
	}

	for(i = 0; i < RANDSIZ; i += 8) /* fill in mm[] with messy stuff */
	{
		if(flag) /* use all the information in the seed */
		{
			a += randrsl[i];
			b += randrsl[i + 1];
			c += randrsl[i + 2];
			d += randrsl[i + 3];
			e += randrsl[i + 4];
			f += randrsl[i + 5];
			g += randrsl[i + 6];
			h += randrsl[i + 7];
		}
		mix(a, b, c, d, e, f, g, h);
		mm[i] = a;
		mm[i + 1] = b;
		mm[i + 2] = c;
		mm[i + 3] = d;
		mm[i + 4] = e;
		mm[i + 5] = f;
		mm[i + 6] = g;
		mm[i + 7] = h;
	}

	if(flag) { /* do a second pass to make all of the seed affect all of mm */
		for(i = 0; i < RANDSIZ; i += 8) {
			a += mm[i];
			b += mm[i + 1];
			c += mm[i + 2];
			d += mm[i + 3];
			e += mm[i + 4];
			f += mm[i + 5];
			g += mm[i + 6];
			h += mm[i + 7];
			mix(a, b, c, d, e, f, g, h);
			mm[i] = a;
			mm[i + 1] = b;
			mm[i + 2] = c;
			mm[i + 3] = d;
			mm[i + 4] = e;
			mm[i + 5] = f;
			mm[i + 6] = g;
			mm[i + 7] = h;
		}
	}

	isaac64();         /* fill in the first set of results */
	randcnt = RANDSIZ; /* prepare to use the first set of results */
}

void rand_csprng_reseed(void *entropy, size_t len)
{
	memset(randrsl, 0, sizeof(randrsl));
	memcpy(randrsl, entropy, len > RANDSIZ ? RANDSIZ : len);
	randinit(1);
}

void rand_csprng_get(void *data, size_t len)
{
	char *d = data;
	for(size_t i = 0; i < len; i++) {
		uint64_t r = rand();
		char *s = (char *)&r;
		for(size_t j = 0; j + i < len && j < 8; j++) {
			*d++ = *s++;
		}
		i += 8;
	}
}
