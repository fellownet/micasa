#ifndef _HKDF_H_
#define _HKDF_H_

#include <openssl/sha.h>

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define SHAInit SHA512_Init
#define SHAUpdate SHA512_Update
#define SHAFinal SHA512_Final
#define SHA_DIGESTSIZE 64
#define SHA_BlockSize 128

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef SHA512_CTX SHACTX;

enum {
    shaSuccess = 0,
    shaNull,            /* Null pointer parameter */
    shaInputTooLong,    /* input data too long */
    shaStateError,      /* called Input after FinalBits or Result */
    shaBadParam         /* passed a bad parameter */
};

/*
 *  This structure will hold context information for the HMAC
 *  keyed-hashing operation.
 */
typedef struct HMACContext {
    int hashSize;               /* hash size of SHA being used */
    int blockSize;              /* block size of SHA being used */
    SHACTX shaContext;     /* SHA context */
    unsigned char k_opad[SHA_BlockSize];
                        /* outer padding - key XORd with opad */
    int Computed;               /* Is the MAC computed? */
    int Corrupted;              /* Cumulative corruption code */

} HMACContext;

/*
 *  This structure will hold context information for the HKDF
 *  extract-and-expand Key Derivation Functions.
 */
typedef struct HKDFContext {
    HMACContext hmacContext;
    int hashSize;               /* hash size of SHA being used */
    unsigned char prk[SHA_DIGESTSIZE];
                        /* pseudo-random key - output of hkdfInput */
    int Computed;               /* Is the key material computed? */
    int Corrupted;              /* Cumulative corruption code */
} HKDFContext;

int hkdf(const unsigned char *salt, int salt_len,
         const unsigned char *ikm, int ikm_len,
         const unsigned char *info, int info_len,
         uint8_t okm[ ], int okm_len);

/*
 * HMAC Keyed-Hashing for Message Authentication, RFC 2104,
 * for all SHAs.
 * This interface allows a fixed-length text input to be used.
 */
int hmac(const unsigned char *text,     /* pointer to data stream */
    int text_len,                  /* length of data stream */
    const unsigned char *key,      /* pointer to authentication key */
    int key_len,                   /* length of authentication key */
    uint8_t digest[SHA_DIGESTSIZE]); /* caller digest to fill in */

/*
 * HKDF HMAC-based Extract-and-Expand Key Derivation Function,
 * RFC 5869, for all SHAs.
 */
 int hkdf(const unsigned char *salt,
	int salt_len, const unsigned char *ikm, int ikm_len,
	const unsigned char *info, int info_len,
	uint8_t okm[ ], int okm_len);
int hkdfExtract(const unsigned char *salt,
		   int salt_len, const unsigned char *ikm,
		   int ikm_len, uint8_t prk[SHA_DIGESTSIZE]);
int hkdfExpand(const uint8_t prk[ ],
		  int prk_len, const unsigned char *info,
		  int info_len, uint8_t okm[ ], int okm_len);

/*
* HKDF HMAC-based Extract-and-Expand Key Derivation Function,
* RFC 5869, for all SHAs.
* This interface allows any length of text input to be used.
*/
int hkdfReset(HKDFContext *context,
		 const unsigned char *salt, int salt_len);
int hkdfInput(HKDFContext *context, const unsigned char *ikm,
		 int ikm_len);
//int hkdfFinalBits(HKDFContext *context, uint8_t ikm_bits,
//			 unsigned int ikm_bit_count);
int hkdfResult(HKDFContext *context,
		  uint8_t prk[SHA_DIGESTSIZE],
		  const unsigned char *info, int info_len,
		  uint8_t okm[SHA_DIGESTSIZE], int okm_len);

/*
 * HMAC Keyed-Hashing for Message Authentication, RFC 2104,
 * for all SHAs.
 * This interface allows any length of text input to be used.
 */
int hmacReset(HMACContext *context,
                     const unsigned char *key, int key_len);
int hmacInput(HMACContext *context, const unsigned char *text,
                     int text_len);
//int hmacFinalBits(HMACContext *context, uint8_t bits,
//                         unsigned int bit_count);
int hmacResult(HMACContext *context,
                      uint8_t digest[SHA_DIGESTSIZE]);


int SHA512Reset(SHACTX *c);
int SHA512Input(SHACTX *c, const void *data, unsigned int len);
int SHA512FinalBits(SHACTX *c, const void *data, unsigned int len);
int SHA512Result(SHA512_CTX *c, unsigned char *md);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _HKDF_H_
