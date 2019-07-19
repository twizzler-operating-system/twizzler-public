/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 */
#include "tomcrypt_private.h"

#ifndef __KERNEL__

#ifdef LTC_RNG_GET_BYTES
/**
   @file rng_get_bytes.c
   portable way to get secure random bits to feed a PRNG (Tom St Denis)
*/

#if defined(LTC_DEVRANDOM) && !defined(_WIN32)
/* on *NIX read /dev/random */
static unsigned long _rng_nix(unsigned char *buf, unsigned long len, void (*callback)(void))
{
#ifdef LTC_NO_FILE
   LTC_UNUSED_PARAM(callback);
   LTC_UNUSED_PARAM(buf);
   LTC_UNUSED_PARAM(len);
   return 0;
#else
   FILE *f;
   unsigned long x;
   LTC_UNUSED_PARAM(callback);
#ifdef LTC_TRY_URANDOM_FIRST
   f = fopen("/dev/urandom", "rb");
   if (f == NULL) {
      f = fopen("/dev/random", "rb");
   }
#else
   f = fopen("/dev/random", "rb");
#endif /* LTC_TRY_URANDOM_FIRST */

   if (f == NULL) {
      return 0;
   }

   /* disable buffering */
   if (setvbuf(f, NULL, _IONBF, 0) != 0) {
      fclose(f);
      return 0;
   }

   x = (unsigned long)fread(buf, 1, (size_t)len, f);
   fclose(f);
   return x;
#endif /* LTC_NO_FILE */
}

#endif /* LTC_DEVRANDOM */

/**
  Read the system RNG
  @param out       Destination
  @param outlen    Length desired (octets)
  @param callback  Pointer to void function to act as "callback" when RNG is slow.  This can be NULL
  @return Number of octets read
*/
unsigned long rng_get_bytes(unsigned char *out, unsigned long outlen, void (*callback)(void))
{
   unsigned long x;

   LTC_ARGCHK(out != NULL);

#ifdef LTC_PRNG_ENABLE_LTC_RNG
   if (ltc_rng) {
      x = ltc_rng(out, outlen, callback);
      if (x != 0) {
         return x;
      }
   }
#endif

#if defined(_WIN32) || defined(_WIN32_WCE)
   x = _rng_win32(out, outlen, callback);
   if (x != 0) {
      return x;
   }
#elif defined(LTC_DEVRANDOM)
   x = _rng_nix(out, outlen, callback);
   if (x != 0) {
      return x;
   }
#endif
#ifdef ANSI_RNG
   x = _rng_ansic(out, outlen, callback);
   if (x != 0) {
      return x;
   }
#endif
   return 0;
}
#endif /* #ifdef LTC_RNG_GET_BYTES */

/* ref:         HEAD -> develop */
/* git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046 */
/* commit time: 2019-06-11 07:55:21 +0200 */

#endif
