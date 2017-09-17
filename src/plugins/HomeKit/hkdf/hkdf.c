/*
 *  Description:
 *      This file implements the HKDF algorithm (HMAC-based
 *      Extract-and-Expand Key Derivation Function, RFC 5869),
 *      expressed in terms of the various SHA algorithms.
 */

#include <string.h>
#include <stdlib.h>

/*
 *  hkdf
 *
 *  Description:
 *      This function will generate keying material using HKDF.
 *
 *  Parameters:
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      salt[ ]: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(whichSha) zeros.
 *      salt_len: [in]
 *          The length of the salt value.  (Ignored if salt == NULL.)
 *      ikm[ ]: [in]
 *          Input keying material.
 *      ikm_len: [in]
 *          The length of the input keying material.
 *      info[ ]: [in]
 *          The optional context and application specific information.
 *          If info == NULL or a zero-length string, it is ignored.
 *      info_len: [in]
 *          The length of the optional context and application specific
 *          information.  (Ignored if info == NULL.)
 *      okm[ ]: [out]
 *          Where the HKDF is to be stored.
 *      okm_len: [in]
 *          The length of the buffer to hold okm.
 *          okm_len must be <= 255 * USHABlockSize(whichSha)
 *
 *  Notes:
 *      Calls hkdfExtract() and hkdfExpand().
 *
 *  Returns:
 *      sha Error Code.
 *
 */
#include "hkdf.h"

int hkdf(const unsigned char *salt, int salt_len,
    const unsigned char *ikm, int ikm_len,
    const unsigned char *info, int info_len,
    uint8_t okm[ ], int okm_len)
{
  uint8_t prk[SHA_DIGESTSIZE];
  return hkdfExtract(salt, salt_len, ikm, ikm_len, prk) ||
         hkdfExpand(prk, SHA_DIGESTSIZE, info,
                    info_len, okm, okm_len);
}

/*
 *  hkdfExtract
 *
 *  Description:
 *      This function will perform HKDF extraction.
 *
 *  Parameters:
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      salt[ ]: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(whichSha) zeros.
 *      salt_len: [in]
 *          The length of the salt value.  (Ignored if salt == NULL.)
 *      ikm[ ]: [in]
 *          Input keying material.
 *      ikm_len: [in]
 *          The length of the input keying material.
 *      prk[ ]: [out]
 *          Array where the HKDF extraction is to be stored.
 *          Must be larger than USHAHashSize(whichSha);
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int hkdfExtract(const unsigned char *salt, int salt_len,
    const unsigned char *ikm, int ikm_len,
    uint8_t prk[SHA_DIGESTSIZE])
{
  unsigned char nullSalt[SHA_DIGESTSIZE];
  if (salt == 0) {
    salt = nullSalt;
    salt_len = SHA_DIGESTSIZE;
    memset(nullSalt, '\0', salt_len);
  } else if (salt_len < 0) {
    return shaBadParam;
  }
  return hmac(ikm, ikm_len, salt, salt_len, prk);
}

/*
 *  hkdfExpand
 *
 *  Description:
 *      This function will perform HKDF expansion.
 *
 *  Parameters:
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      prk[ ]: [in]
 *          The pseudo-random key to be expanded; either obtained
 *          directly from a cryptographically strong, uniformly
 *          distributed pseudo-random number generator, or as the
 *          output from hkdfExtract().
 *      prk_len: [in]
 *          The length of the pseudo-random key in prk;
 *          should at least be equal to USHAHashSize(whichSHA).
 *      info[ ]: [in]
 *          The optional context and application specific information.
 *          If info == NULL or a zero-length string, it is ignored.
 *      info_len: [in]
 *          The length of the optional context and application specific
 *          information.  (Ignored if info == NULL.)
 *      okm[ ]: [out]
 *          Where the HKDF is to be stored.
 *      okm_len: [in]
 *          The length of the buffer to hold okm.
 *          okm_len must be <= 255 * USHABlockSize(whichSha)
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int hkdfExpand(const uint8_t prk[ ], int prk_len,
    const unsigned char *info, int info_len,
    uint8_t okm[ ], int okm_len)
{
  int hash_len, N;
  unsigned char T[SHA_DIGESTSIZE];
  int Tlen, where, i;

  if (info == 0) {
    info = (const unsigned char *)"";
    info_len = 0;
  } else if (info_len < 0) {
    return shaBadParam;
  }
  if (okm_len <= 0) return shaBadParam;
  if (!okm) return shaBadParam;

  hash_len = SHA_DIGESTSIZE;
  if (prk_len < hash_len) return shaBadParam;
  N = okm_len / hash_len;
  if ((okm_len % hash_len) != 0) N++;
  if (N > 255) return shaBadParam;

  Tlen = 0;
  where = 0;
  for (i = 1; i <= N; i++) {
    HMACContext context;
    unsigned char c = i;
      int ret = hmacReset(&context, prk, prk_len) ||
      hmacInput(&context, T, Tlen) ||
              hmacInput(&context, info, info_len) ||
              hmacInput(&context, &c, 1) ||
              hmacResult(&context, T);
    if (ret != shaSuccess) return ret;
    memcpy(okm + where, T,
           (i != N) ? hash_len : (okm_len - where));
    where += hash_len;
    Tlen = hash_len;
  }
  return shaSuccess;
}

/*
 *  hkdfReset
 *
 *  Description:
 *      This function will initialize the hkdfContext in preparation
 *      for key derivation using the modular HKDF interface for
 *      arbitrary length inputs.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to reset.
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      salt[ ]: [in]
 *          The optional salt value (a non-secret random value);
 *          if not provided (salt == NULL), it is set internally
 *          to a string of HashLen(whichSha) zeros.
 *      salt_len: [in]
 *          The length of the salt value.  (Ignored if salt == NULL.)
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int hkdfReset(HKDFContext *context,
              const unsigned char *salt, int salt_len)
{
  unsigned char nullSalt[SHA_DIGESTSIZE];
  if (!context) return shaNull;

  context->hashSize = SHA_DIGESTSIZE;
  if (salt == 0) {
    salt = nullSalt;
    salt_len = context->hashSize;
    memset(nullSalt, '\0', salt_len);
  }

  return hmacReset(&context->hmacContext, salt, salt_len);
}

/*
 *  hkdfInput
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the input keying material.  It may be called multiple times.
 *
 *  Parameters:
 *      context: [in/out]
 *          The HKDF context to update.
 *      ikm[ ]: [in]
 *          An array of octets representing the next portion of
 *          the input keying material.
 *      ikm_len: [in]
 *          The length of ikm.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int hkdfInput(HKDFContext *context, const unsigned char *ikm,
              int ikm_len)
{
  if (!context) return shaNull;
  if (context->Corrupted) return context->Corrupted;
  if (context->Computed) return context->Corrupted = shaStateError;
  return hmacInput(&context->hmacContext, ikm, ikm_len);
}

/*
 * hkdfFinalBits
 *
 * Description:
 *   This function will add in any final bits of the
 *   input keying material.
 *
 * Parameters:
 *   context: [in/out]
 *     The HKDF context to update
 *   ikm_bits: [in]
 *     The final bits of the input keying material, in the upper
 *     portion of the byte.  (Use 0b###00000 instead of 0b00000###
 *     to input the three bits ###.)
 *   ikm_bit_count: [in]
 *     The number of bits in message_bits, between 1 and 7.
 *
 * Returns:
 *   sha Error Code.
 */
/*
int hkdfFinalBits(HKDFContext *context, uint8_t ikm_bits,
                  unsigned int ikm_bit_count)
{
  if (!context) return shaNull;
  if (context->Corrupted) return context->Corrupted;
  if (context->Computed) return context->Corrupted = shaStateError;
  return hmacFinalBits(&context->hmacContext, ikm_bits, ikm_bit_count);
}
*/

/*
 * hkdfResult
 *
 * Description:
 *   This function will finish the HKDF extraction and perform the
 *   final HKDF expansion.
 *
 * Parameters:
 *   context: [in/out]
 *     The HKDF context to use to calculate the HKDF hash.
 *   prk[ ]: [out]
 *     An optional location to store the HKDF extraction.
 *     Either NULL, or pointer to a buffer that must be
 *     larger than USHAHashSize(whichSha);
 *   info[ ]: [in]
 *     The optional context and application specific information.
 *     If info == NULL or a zero-length string, it is ignored.
 *   info_len: [in]
 *     The length of the optional context and application specific
 *     information.  (Ignored if info == NULL.)
 *   okm[ ]: [out]
 *     Where the HKDF is to be stored.
 *   okm_len: [in]
 *     The length of the buffer to hold okm.
 *     okm_len must be <= 255 * USHABlockSize(whichSha)
 *
 * Returns:
 *   sha Error Code.
 *
 */
int hkdfResult(HKDFContext *context,
               uint8_t prk[SHA_DIGESTSIZE],
               const unsigned char *info, int info_len,
               uint8_t okm[ ], int okm_len)
{
  uint8_t prkbuf[SHA_DIGESTSIZE];
  int ret;

  if (!context) return shaNull;
  if (context->Corrupted) return context->Corrupted;
  if (context->Computed) return context->Corrupted = shaStateError;
  if (!okm) return context->Corrupted = shaBadParam;
  if (!prk) prk = prkbuf;

  ret = hmacResult(&context->hmacContext, prk) ||
        hkdfExpand(prk, context->hashSize, info,
                   info_len, okm, okm_len);
  context->Computed = 1;
  return context->Corrupted = ret;
}


/*
 *  hmac
 *
 *  Description:
 *      This function will compute an HMAC message digest.
 *
 *  Parameters:
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      message_array[ ]: [in]
 *          An array of octets representing the message.
 *          Note: in RFC 2104, this parameter is known
 *          as 'text'.
 *      length: [in]
 *          The length of the message in message_array.
 *      key[ ]: [in]
 *          The secret shared key.
 *      key_len: [in]
 *          The length of the secret shared key.
 *      digest[ ]: [out]
 *          Where the digest is to be returned.
 *          NOTE: The length of the digest is determined by
 *              the value of whichSha.
 *
 *  Returns:
 *      sha Error Code.
 *
 */

 int hmac(const unsigned char *message_array, int length,
    const unsigned char *key, int key_len,
    unsigned char digest[SHA_DIGESTSIZE])
{
  HMACContext context;
  return hmacReset(&context, key, key_len) ||
         hmacInput(&context, message_array, length) ||
         hmacResult(&context, digest);
}

/*
 *  hmacReset
 *
 *  Description:
 *      This function will initialize the hmacContext in preparation
 *      for computing a new HMAC message digest.
 *
 *  Parameters:
 *      context: [in/out]
 *          The context to reset.
 *      whichSha: [in]
 *          One of SHA1, SHA224, SHA256, SHA384, SHA512
 *      key[ ]: [in]
 *          The secret shared key.
 *      key_len: [in]
 *          The length of the secret shared key.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int hmacReset(HMACContext *context,
    const unsigned char *key, int key_len)
{
  int i, blocksize, hashsize, ret;

  /* inner padding - key XORd with ipad */
  unsigned char k_ipad[SHA_BlockSize];

  /* temporary buffer when keylen > blocksize */
  unsigned char tempkey[SHA512_DIGEST_LENGTH];

  if (!context) return shaNull;
  context->Computed = 0;
  context->Corrupted = shaSuccess;

  blocksize = context->blockSize = SHA_BlockSize;
  hashsize = context->hashSize = SHA_DIGESTSIZE;

  /*
   * If key is longer than the hash blocksize,
   * reset it to key = HASH(key).
   */
  if (key_len > blocksize) {
    SHACTX tcontext;
    int err = SHA512Reset(&tcontext) ||
              SHA512Input(&tcontext, key, key_len) ||
              SHA512Result(&tcontext, tempkey);
    if (err != shaSuccess) return err;

    key = tempkey;
    key_len = hashsize;
  }

  /*
   * The HMAC transform looks like:
   *
   * SHA(K XOR opad, SHA(K XOR ipad, text))
   *
   * where K is an n byte key, 0-padded to a total of blocksize bytes,
   * ipad is the byte 0x36 repeated blocksize times,
   * opad is the byte 0x5c repeated blocksize times,
   * and text is the data being protected.
   */

  /* store key into the pads, XOR'd with ipad and opad values */
  for (i = 0; i < key_len; i++) {
    k_ipad[i] = key[i] ^ 0x36;
    context->k_opad[i] = key[i] ^ 0x5c;
  }
  /* remaining pad bytes are '\0' XOR'd with ipad and opad values */
  for ( ; i < blocksize; i++) {
    k_ipad[i] = 0x36;
    context->k_opad[i] = 0x5c;
  }

  /* perform inner hash */
  /* init context for 1st pass */
  ret = SHA512Reset(&context->shaContext) ||
        /* and start with inner pad */
        SHA512Input(&context->shaContext, k_ipad, blocksize);
  return context->Corrupted = ret;
}

/*
 *  hmacInput
 *
 *  Description:
 *      This function accepts an array of octets as the next portion
 *      of the message.  It may be called multiple times.
 *
 *  Parameters:
 *      context: [in/out]
 *          The HMAC context to update.
 *      text[ ]: [in]
 *          An array of octets representing the next portion of
 *          the message.
 *      text_len: [in]
 *          The length of the message in text.
 *
 *  Returns:
 *      sha Error Code.
 *
 */
int hmacInput(HMACContext *context, const unsigned char *text,
    int text_len)
{
  if (!context) return shaNull;
  if (context->Corrupted) return context->Corrupted;
  if (context->Computed) return context->Corrupted = shaStateError;
  /* then text of datagram */
  return context->Corrupted =
    SHA512Input(&context->shaContext, text, text_len);
}

/*
 * hmacFinalBits
 *
 * Description:
 *   This function will add in any final bits of the message.
 *
 * Parameters:
 *   context: [in/out]
 *     The HMAC context to update.
 *   message_bits: [in]
 *     The final bits of the message, in the upper portion of the
 *     byte.  (Use 0b###00000 instead of 0b00000### to input the
 *     three bits ###.)
 *   length: [in]
 *     The number of bits in message_bits, between 1 and 7.
 *
 * Returns:
 *   sha Error Code.
 */
//int hmacFinalBits(HMACContext *context,
//    uint8_t bits, unsigned int bit_count)
//{
//  if (!context) return shaNull;
//  if (context->Corrupted) return context->Corrupted;
//  if (context->Computed) return context->Corrupted = shaStateError;
//  /* then final bits of datagram */
//  return context->Corrupted =
//    SHA512FinalBits(&context->shaContext, bits, bit_count);
//}

/*
 * hmacResult
 *
 * Description:
 *   This function will return the N-byte message digest into the
 *   Message_Digest array provided by the caller.
 *
 * Parameters:
 *   context: [in/out]
 *     The context to use to calculate the HMAC hash.
 *   digest[ ]: [out]
 *     Where the digest is returned.
 *     NOTE 2: The length of the hash is determined by the value of
 *      whichSha that was passed to hmacReset().
 *
 * Returns:
 *   sha Error Code.
 *
 */
int hmacResult(HMACContext *context, uint8_t *digest)
{
  int ret;
  if (!context) return shaNull;
  if (context->Corrupted) return context->Corrupted;
  if (context->Computed) return context->Corrupted = shaStateError;

  /* finish up 1st pass */
  /* (Use digest here as a temporary buffer.) */
  ret =
    SHA512Result(&context->shaContext, digest) ||

         /* perform outer SHA */
         /* init context for 2nd pass */
         SHA512Reset(&context->shaContext) ||

         /* start with outer pad */
         SHA512Input(&context->shaContext, context->k_opad,
                   context->blockSize) ||

         /* then results of 1st hash */
         SHA512Input(&context->shaContext, digest, context->hashSize) ||
         /* finish up 2nd pass */
         SHA512Result(&context->shaContext, digest);

  context->Computed = 1;
  return context->Corrupted = ret;
}


int SHA512Reset(SHACTX *c) {
    SHAInit(c);
    return 0;
}
int SHA512Input(SHACTX *c, const void *data, unsigned int len) {
    SHAUpdate(c, data, len);
    return 0;
}
int SHA512FinalBits(SHACTX *c, const void *data, unsigned int len) {
    SHAUpdate(c, data, len);
    return 0;
}
int SHA512Result(SHACTX *c, unsigned char *md) {
    SHAFinal(md, c);
    return 0;
}
