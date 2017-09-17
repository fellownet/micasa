#ifndef _POLY1305_H_
#define _POLY1305_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct poly1305_context {
	size_t aligner;
	unsigned char opaque[136];
} poly1305_context;

typedef struct poly1305_context poly1305_state;

typedef enum {
	Type_Data_Without_Length      = 1,
	Type_Data_With_Length         = 2,
} Poly1305Type_t;

void poly1305_init(poly1305_context *ctx, const unsigned char key[32]);
void poly1305_update(poly1305_context *ctx, const unsigned char *in, size_t len);
void poly1305_finish(poly1305_context *ctx, unsigned char mac[16]);
void poly1305_genkey(const unsigned char * key, uint8_t * buf, uint16_t len, Poly1305Type_t type, char* verify);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _POLY1305_H_
