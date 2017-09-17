#ifndef ED25519_DONNA_H
#define ED25519_DONNA_H

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef uint32_t bignum25519[10];

typedef uint32_t bignum256modm_element_t;
typedef bignum256modm_element_t bignum256modm[9];

typedef struct ge25519_t {
	bignum25519 x, y, z, t;
} ge25519;

typedef struct ge25519_p1p1_t {
	bignum25519 x, y, z, t;
} ge25519_p1p1;

typedef struct ge25519_niels_t {
	bignum25519 ysubx, xaddy, t2d;
} ge25519_niels;

typedef struct ge25519_pniels_t {
	bignum25519 ysubx, xaddy, z, t2d;
} ge25519_pniels;

int ed25519_verify(const unsigned char *x, const unsigned char *y, size_t len);

void curve25519_pow_two5mtwo0_two250mtwo0(bignum25519 b);
void curve25519_recip(bignum25519 out, const bignum25519 z);
void curve25519_pow_two252m3(bignum25519 two252m3, const bignum25519 z);

void curve25519_copy(bignum25519 out, const bignum25519 in);
void curve25519_add(bignum25519 out, const bignum25519 a, const bignum25519 b);
void curve25519_add_after_basic(bignum25519 out, const bignum25519 a, const bignum25519 b);
void curve25519_add_reduce(bignum25519 out, const bignum25519 a, const bignum25519 b);

void curve25519_sub(bignum25519 out, const bignum25519 a, const bignum25519 b);
void curve25519_sub_after_basic(bignum25519 out, const bignum25519 a, const bignum25519 b);
void curve25519_sub_reduce(bignum25519 out, const bignum25519 a, const bignum25519 b);
void curve25519_neg(bignum25519 out, const bignum25519 a);

void curve25519_mul(bignum25519 out, const bignum25519 a, const bignum25519 b);
void curve25519_square(bignum25519 out, const bignum25519 in);
void curve25519_square_times(bignum25519 out, const bignum25519 in, int count);
void curve25519_expand(bignum25519 out, const unsigned char in[32]);
void curve25519_contract(unsigned char out[32], const bignum25519 in);

void curve25519_move_conditional_bytes(uint8_t out[96], const uint8_t in[96], uint32_t flag);
void curve25519_swap_conditional(bignum25519 a, bignum25519 b, uint32_t iswap);

bignum256modm_element_t lt_modm(bignum256modm_element_t a, bignum256modm_element_t b);

void reduce256_modm(bignum256modm r);
void barrett_reduce256_modm(bignum256modm r, const bignum256modm q1, const bignum256modm r1);
void add256_modm(bignum256modm r, const bignum256modm x, const bignum256modm y);
void mul256_modm(bignum256modm r, const bignum256modm x, const bignum256modm y);
void expand256_modm(bignum256modm out, const unsigned char *in, size_t len);
void expand_raw256_modm(bignum256modm out, const unsigned char in[32]);
void contract256_modm(unsigned char out[32], const bignum256modm in);
void contract256_window4_modm(signed char r[64], const bignum256modm in);
void contract256_slidingwindow_modm(signed char r[256], const bignum256modm s, int windowsize);

void ge25519_p1p1_to_partial(ge25519 *r, const ge25519_p1p1 *p);
void ge25519_p1p1_to_full(ge25519 *r, const ge25519_p1p1 *p);
void ge25519_full_to_pniels(ge25519_pniels *p, const ge25519 *r);
void ge25519_add_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519 *q);
void ge25519_double_p1p1(ge25519_p1p1 *r, const ge25519 *p);
void ge25519_nielsadd2_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519_niels *q, unsigned char signbit);
void ge25519_pnielsadd_p1p1(ge25519_p1p1 *r, const ge25519 *p, const ge25519_pniels *q, unsigned char signbit);
void ge25519_double_partial(ge25519 *r, const ge25519 *p);
void ge25519_double(ge25519 *r, const ge25519 *p);
void ge25519_add(ge25519 *r, const ge25519 *p,  const ge25519 *q);
void ge25519_nielsadd2(ge25519 *r, const ge25519_niels *q);
void ge25519_pnielsadd(ge25519_pniels *r, const ge25519 *p, const ge25519_pniels *q);
void ge25519_pack(unsigned char r[32], const ge25519 *p);
int ge25519_unpack_negative_vartime(ge25519 *r, const unsigned char p[32]);

void ge25519_double_scalarmult_vartime(ge25519 *r, const ge25519 *p1, const bignum256modm s1, const bignum256modm s2);

uint32_t ge25519_windowb_equal(uint32_t b, uint32_t c);
void ge25519_scalarmult_base_choose_niels(ge25519_niels *t, const uint8_t table[256][96], uint32_t pos, signed char b);

void ge25519_scalarmult_base_niels(ge25519 *r, const uint8_t basepoint_table[256][96], const bignum256modm s);

void U32TO8_LE(unsigned char *p, const uint32_t v);
uint32_t U8TO32_LE(const unsigned char *p);

#if defined(__cplusplus)
}
#endif

#endif // ED25519_DONNA_H
