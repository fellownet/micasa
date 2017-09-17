#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>

#include "openssl/opensslv.h"
#include "openssl/bn.h"

typedef BIGNUM * BigInteger;
typedef BN_CTX * BigIntegerCtx;
typedef BN_MONT_CTX * BigIntegerModAccel;
typedef int (*modexp_meth)(BIGNUM *r, const BIGNUM *a, const BIGNUM *p,
			   const BIGNUM *m, BN_CTX *ctx, BN_MONT_CTX *mctx);
static modexp_meth default_modexp = NULL;

#define MATH_PRIV

#include "srp_aux.h"

/* Math library interface stubs */

BigInteger
BigIntegerFromInt(n)
     unsigned int n;
{
  BIGNUM * a = BN_new();
  if(a)
    BN_set_word(a, n);
  return a;
}

BigInteger
BigIntegerFromBytes(bytes, length)
     const unsigned char * bytes;
     int length;
{
  BIGNUM * a = BN_new();
  BN_bin2bn(bytes, length, a);
  return a;
}

int
BigIntegerToBytes(src, dest, destlen)
     BigInteger src;
     unsigned char * dest;
     int destlen;
{
  return BN_bn2bin(src, dest);
}

BigIntegerResult
BigIntegerToCstr(BigInteger x, cstr * out)
{
  int n = BigIntegerByteLen(x);
  if(cstr_set_length(out, n) < 0)
    return BIG_INTEGER_ERROR;
  if(cstr_set_length(out, BigIntegerToBytes(x, out->data, n)) < 0)
    return BIG_INTEGER_ERROR;
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerToCstrEx(BigInteger x, cstr * out, int len)
{
  int n;
  if(cstr_set_length(out, len) < 0)
    return BIG_INTEGER_ERROR;
  n = BigIntegerToBytes(x, out->data, len);
  if(n < len) {
    memmove(out->data + (len - n), out->data, n);
    memset(out->data, 0, len - n);
  }
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerToHex(src, dest, destlen)
     BigInteger src;
     char * dest;
     int destlen;
{
  strcpy(dest, BN_bn2hex(src));
  return BIG_INTEGER_SUCCESS;
}

static char b64table[] =
  "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz./";

BigIntegerResult
BigIntegerToString(src, dest, destlen, radix)
     BigInteger src;
     char * dest;
     int destlen;
     unsigned int radix;
{
  BigInteger t = BigIntegerFromInt(0);
  char * p = dest;
  char c;

  *p++ = b64table[BigIntegerModInt(src, radix, NULL)];
  BigIntegerDivInt(t, src, radix, NULL);
  while(BigIntegerCmpInt(t, 0) > 0) {
    *p++ = b64table[BigIntegerModInt(t, radix, NULL)];
    BigIntegerDivInt(t, t, radix, NULL);
  }
  BigIntegerFree(t);
  *p-- = '\0';
  /* reverse the string */
  while(p > dest) {
    c = *p;
    *p-- =  *dest;
    *dest++ = c;
  }
  return BIG_INTEGER_SUCCESS;
}

int
BigIntegerBitLen(b)
     BigInteger b;
{
  return BN_num_bits(b);
}

int
BigIntegerCmp(c1, c2)
     BigInteger c1, c2;
{
  return BN_cmp(c1, c2);
}

int
BigIntegerCmpInt(c1, c2)
     BigInteger c1;
     unsigned int c2;
{
// BOB
// ->top and ->d have been abstracted away since openssl 1.1.x
# if OPENSSL_VERSION_NUMBER >= 0x10100000
  BIGNUM *bn_c2 = BN_new();
  BN_set_word( bn_c2, c2 );
  int result = BN_cmp( c1, bn_c2 );
  BN_free( bn_c2 );
  return result;
# else
  if(c1->top > 1)
    return 1;
  else if(c1->top < 1)
    return (c2 > 0) ? -1 : 0;
  else {
    if(c1->d[0] > c2)
      return 1;
    else if(c1->d[0] < c2)
      return -1;
    else
      return 0;
  }
# endif
}

BigIntegerResult
BigIntegerLShift(result, x, bits)
     BigInteger result, x;
     unsigned int bits;
{
  BN_lshift(result, x, bits);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerAdd(result, a1, a2)
     BigInteger result, a1, a2;
{
  BN_add(result, a1, a2);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerAddInt(result, a1, a2)
     BigInteger result, a1;
     unsigned int a2;
{
  if(result != a1)
    BN_copy(result, a1);
  BN_add_word(result, a2);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerSub(result, s1, s2)
     BigInteger result, s1, s2;
{
  BN_sub(result, s1, s2);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerSubInt(result, s1, s2)
     BigInteger result, s1;
     unsigned int s2;
{
  if(result != s1)
    BN_copy(result, s1);
  BN_sub_word(result, s2);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerMul(result, m1, m2, c)
     BigInteger result, m1, m2;
     BigIntegerCtx c;
{
  BN_CTX * ctx = NULL;
  if(c == NULL)
    c = ctx = BN_CTX_new();
  BN_mul(result, m1, m2, c);
  if(ctx)
    BN_CTX_free(ctx);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerMulInt(result, m1, m2, c)
     BigInteger result, m1;
     unsigned int m2;
     BigIntegerCtx c;
{
  if(result != m1)
    BN_copy(result, m1);
  BN_mul_word(result, m2);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerDivInt(result, d, m, c)
     BigInteger result, d;
     unsigned int m;
     BigIntegerCtx c;
{
  if(result != d)
    BN_copy(result, d);
  BN_div_word(result, m);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerMod(result, d, m, c)
     BigInteger result, d, m;
     BigIntegerCtx c;
{
  BN_CTX * ctx = NULL;
  if(c == NULL)
    c = ctx = BN_CTX_new();
  BN_mod(result, d, m, c);
  if(ctx)
    BN_CTX_free(ctx);
  return BIG_INTEGER_SUCCESS;
}

unsigned int
BigIntegerModInt(d, m, c)
     BigInteger d;
     unsigned int m;
     BigIntegerCtx c;
{
  return BN_mod_word(d, m);
}

BigIntegerResult
BigIntegerModMul(r, m1, m2, modulus, c)
     BigInteger r, m1, m2, modulus;
     BigIntegerCtx c;
{
  BN_CTX * ctx = NULL;
  if(c == NULL)
    c = ctx = BN_CTX_new();
  BN_mod_mul(r, m1, m2, modulus, c);
  if(ctx)
    BN_CTX_free(ctx);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerModExp(r, b, e, m, c, a)
     BigInteger r, b, e, m;
     BigIntegerCtx c;
     BigIntegerModAccel a;
{
  BN_CTX * ctx = NULL;
  if(c == NULL)
    c = ctx = BN_CTX_new();
  if(default_modexp) {
    (*default_modexp)(r, b, e, m, c, a);
  }
  else if(a == NULL) {
    BN_mod_exp(r, b, e, m, c);
  }
#if OPENSSL_VERSION_NUMBER >= 0x00906000
// BOB
// ->top and ->d have been abstracted away since openssl 1.1.x
# if OPENSSL_VERSION_NUMBER >= 0x10100000
# else
  else if(b->top == 1) {  /* 0.9.6 and above has mont_word optimization */
    BN_ULONG B = b->d[0];
    BN_mod_exp_mont_word(r, B, e, m, c, a);
  }
# endif
#endif
  else
    BN_mod_exp_mont(r, b, e, m, c, a);
  if(ctx)
    BN_CTX_free(ctx);
  return BIG_INTEGER_SUCCESS;
}

int
BigIntegerCheckPrime(n, c)
     BigInteger n;
     BigIntegerCtx c;
{
  int rv;
  BN_CTX * ctx = NULL;
  if(c == NULL)
    c = ctx = BN_CTX_new();
  rv = BN_is_prime_ex(n, 25, c, NULL);

  if(ctx)
    BN_CTX_free(ctx);
  return rv;
}

BigIntegerResult
BigIntegerFree(b)
     BigInteger b;
{
  BN_free(b);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerClearFree(b)
     BigInteger b;
{
  BN_clear_free(b);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerCtx
BigIntegerCtxNew()
{
  return BN_CTX_new();
}

BigIntegerResult
BigIntegerCtxFree(ctx)
     BigIntegerCtx ctx;
{
  if(ctx)
    BN_CTX_free(ctx);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerModAccel
BigIntegerModAccelNew(m, c)
     BigInteger m;
     BigIntegerCtx c;
{
  BN_CTX * ctx = NULL;
  BN_MONT_CTX * mctx;
  if(default_modexp)
    return NULL;
  if(c == NULL)
    c = ctx = BN_CTX_new();
  mctx = BN_MONT_CTX_new();
  BN_MONT_CTX_set(mctx, m, c);
  if(ctx)
    BN_CTX_free(ctx);
  return mctx;
}

BigIntegerResult
BigIntegerModAccelFree(accel)
     BigIntegerModAccel accel;
{
  if(accel)
    BN_MONT_CTX_free(accel);
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerInitialize()
{
  return BIG_INTEGER_SUCCESS;
}

BigIntegerResult
BigIntegerFinalize()
{
  return BigIntegerReleaseEngine();
}

BigIntegerResult
BigIntegerUseEngine(const char * engine)
{
  return BIG_INTEGER_ERROR;
}

BigIntegerResult
BigIntegerReleaseEngine()
{
  return BIG_INTEGER_SUCCESS;
}
