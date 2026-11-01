#pragma once

#include <CommonCrypto/CommonCryptor.h>
#include <string.h>
#include <stdint.h>

#define MBEDTLS_AES_ENCRYPT 1
#define MBEDTLS_AES_DECRYPT 0

struct mbedtls_aes_context {
  uint8_t key[16];
};

inline void mbedtls_aes_init(mbedtls_aes_context* ctx) { memset(ctx, 0, sizeof(*ctx)); }
inline void mbedtls_aes_free(mbedtls_aes_context* ctx) { memset(ctx, 0, sizeof(*ctx)); }
inline int mbedtls_aes_setkey_enc(mbedtls_aes_context* ctx, const unsigned char* key, unsigned) {
  memcpy(ctx->key, key, 16);
  return 0;
}
inline int mbedtls_aes_setkey_dec(mbedtls_aes_context* ctx, const unsigned char* key, unsigned) {
  memcpy(ctx->key, key, 16);
  return 0;
}

inline int mbedtls_aes_crypt_cbc(mbedtls_aes_context* ctx, int mode, size_t length,
                                 unsigned char iv[16], const unsigned char* input,
                                 unsigned char* output) {
  if (length == 0 || (length % 16) != 0) return -1;
  uint8_t tmp[16];
  uint8_t localIv[16];
  memcpy(localIv, iv, 16);
  if (mode == MBEDTLS_AES_ENCRYPT) {
    for (size_t off = 0; off < length; off += 16) {
      for (int i = 0; i < 16; ++i) tmp[i] = static_cast<uint8_t>(input[off + i] ^ localIv[i]);
      size_t moved = 0;
      if (CCCrypt(kCCEncrypt, kCCAlgorithmAES, 0, ctx->key, 16, nullptr,
                  tmp, 16, output + off, 16, &moved) != kCCSuccess || moved != 16) {
        return -1;
      }
      memcpy(localIv, output + off, 16);
    }
  } else {
    for (size_t off = 0; off < length; off += 16) {
      memcpy(tmp, input + off, 16);
      size_t moved = 0;
      if (CCCrypt(kCCDecrypt, kCCAlgorithmAES, 0, ctx->key, 16, nullptr,
                  input + off, 16, output + off, 16, &moved) != kCCSuccess || moved != 16) {
        return -1;
      }
      for (int i = 0; i < 16; ++i) output[off + i] ^= localIv[i];
      memcpy(localIv, tmp, 16);
    }
  }
  memcpy(iv, localIv, 16);
  return 0;
}
