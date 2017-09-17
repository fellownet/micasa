#ifndef T_SHA_H
#define T_SHA_H

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/sha.h>

typedef SHA512_CTX SHACTX;
#define SHAInit SHA512_Init
#define SHAUpdate SHA512_Update
#define SHAFinal SHA512_Final
#define SHA_DIGESTSIZE 64
#define SHA_BlockSize 128

#endif // T_SHA_H
