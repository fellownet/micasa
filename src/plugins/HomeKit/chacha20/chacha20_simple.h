#ifndef _CHACHA20_SIMPLE_H_
#define _CHACHA20_SIMPLE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
	uint32_t schedule[16];
	uint32_t keystream[16];
	size_t available;
} chacha20_ctx;

void chacha20_setup(chacha20_ctx *ctx, const uint8_t *key, size_t length, uint8_t nonce[8]);
void chacha20_counter_set(chacha20_ctx *ctx, uint64_t counter);
void chacha20_block(chacha20_ctx *ctx, uint32_t output[16]);
void chacha20_encrypt(chacha20_ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);
void chacha20_decrypt(chacha20_ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _CHACHA20_SIMPLE_H_
