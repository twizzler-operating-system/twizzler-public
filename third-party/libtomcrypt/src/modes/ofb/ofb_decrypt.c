/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 */
#include "tomcrypt_private.h"

/**
  @file ofb_decrypt.c
  OFB implementation, decrypt data, Tom St Denis
*/

#ifdef LTC_OFB_MODE

/**
   OFB decrypt
   @param ct      Ciphertext
   @param pt      [out] Plaintext
   @param len     Length of ciphertext (octets)
   @param ofb     OFB state
   @return CRYPT_OK if successful
*/
int ofb_decrypt(const unsigned char *ct, unsigned char *pt, unsigned long len, symmetric_OFB *ofb)
{
   LTC_ARGCHK(pt != NULL);
   LTC_ARGCHK(ct != NULL);
   LTC_ARGCHK(ofb != NULL);
   return ofb_encrypt(ct, pt, len, ofb);
}


#endif



/* ref:         HEAD -> develop */
/* git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046 */
/* commit time: 2019-06-11 07:55:21 +0200 */
