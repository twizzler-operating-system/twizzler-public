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
  @file crypt_hash_is_valid.c
  Determine if hash is valid, Tom St Denis
*/

/*
   Test if a hash index is valid
   @param idx   The index of the hash to search for
   @return CRYPT_OK if valid
*/
int hash_is_valid(int idx)
{
   LTC_MUTEX_LOCK(&ltc_hash_mutex);
   if (idx < 0 || idx >= TAB_SIZE || hash_descriptor[idx].name == NULL) {
      LTC_MUTEX_UNLOCK(&ltc_hash_mutex);
      return CRYPT_INVALID_HASH;
   }
   LTC_MUTEX_UNLOCK(&ltc_hash_mutex);
   return CRYPT_OK;
}

/* ref:         HEAD -> develop */
/* git commit:  e01e4c5c972ba5337d7ab897173fde6e5f0dd046 */
/* commit time: 2019-06-11 07:55:21 +0200 */
