#define LTM_DESC
#include <tomcrypt.h>

#include <err.h>

static void sign_dsa(unsigned char *hash,
  size_t hl,
  const unsigned char *b64data,
  size_t b64len,
  unsigned char *sig,
  size_t *siglen)
{
	register_prng(&sprng_desc);
	ltc_mp = ltm_desc;
	size_t kdlen = b64len;
	unsigned char *kd = malloc(b64len);
	int e;
	if((e = base64_decode(b64data, b64len, kd, &kdlen)) != CRYPT_OK) {
		errx(1, "b64_decode: %s", error_to_string(e));
	}
	dsa_key k;
	if((e = dsa_import(kd, kdlen, &k)) != CRYPT_OK) {
		errx(1, "dsa_import: %s", error_to_string(e));
	}

	if((e = dsa_sign_hash(hash, hl, sig, siglen, NULL, find_prng("sprng"), &k)) != CRYPT_OK) {
		errx(1, "dsa_sign_hash: %s", error_to_string(e));
	}

	int stat;
	if((e = dsa_verify_hash(sig, *siglen, hash, hl, &stat, &k)) != CRYPT_OK) {
		errx(1, "dsa_verify_hash: %s", error_to_string(e));
	}

	if(!stat) {
		errx(1, "failed to test validate signature");
	}
}
