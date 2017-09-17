#ifndef _CURVE25519_DONNA_H_
#define _CURVE25519_DONNA_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef uint8_t u8;

int curve25519_donna(u8 *, const u8 *, const u8 *);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _CURVE25519_DONNA_H_
