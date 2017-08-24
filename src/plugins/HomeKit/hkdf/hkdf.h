//
//  hkdf.h
//  Workbench
//
//  Created by Wai Man Chan on 17/9/14.
//
//

#ifndef Workbench_hkdf_h
#define Workbench_hkdf_h

//#include "../Configuration.h"
#define HomeKitLog 0
#define HomeKitReplyHeaderLog 0
#define PowerOnTest 0

//Device Setting
#define deviceName "House Sensor"    //Name
#define deviceIdentity "11:10:34:23:51:12"  //ID
#define _manufactuerName "ET Chan"   //Manufactuer
#define devicePassword "523-12-643" //Password
#define deviceUUID "62F47751-8F26-46BF-9552-8F4238E67D60"   //UUID, for pair verify
#define controllerRecordsAddress "./PHK_controller" //Where to store the client keys

//Number of client
/*
 * BEWARE: Never set the number of client to 1
 * iOS HomeKit pair setup socket will not release until the pair verify stage start
 * So you will never got the pair corrected, as it is incomplete (The error require manually reset HomeKit setting
 */
#define numberOfClient 20
//Number of notifiable value
/*
 * Count how many notifiable value exist in your set
 * For dynamic add/drop model, please estimate the maximum number (Too few->Buffer overflow)
 */
#define numberOfNotifiableValue 2

#define keepAlivePeriod 60

//If you compiling this to microcontroller, set it to 1
#define MCU 0

#include <openssl/sha.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

typedef SHA512_CTX SHACTX;
#define SHAInit SHA512_Init
#define SHAUpdate SHA512_Update
#define SHAFinal SHA512_Final
#define SHA_DIGESTSIZE 64
#define SHA_BlockSize 128

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
int hkdfFinalBits(HKDFContext *context, uint8_t ikm_bits,
			 unsigned int ikm_bit_count);
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
int hmacFinalBits(HMACContext *context, uint8_t bits,
                         unsigned int bit_count);
int hmacResult(HMACContext *context,
                      uint8_t digest[SHA_DIGESTSIZE]);


int SHA512Reset(SHACTX *c);
int SHA512Input(SHACTX *c, const void *data, unsigned int len);
int SHA512FinalBits(SHACTX *c, const void *data, unsigned int len);
int SHA512Result(SHA512_CTX *c, unsigned char *md);

 #endif
