#pragma once

#include <CommonCrypto/CommonDigest.h>
#include <string.h>
#include <stdint.h>

typedef CC_MD5_CTX mbedtls_md5_context;

inline void mbedtls_md5_init(mbedtls_md5_context*) {}
inline void mbedtls_md5_free(mbedtls_md5_context*) {}
inline int mbedtls_md5_starts(mbedtls_md5_context* ctx) {
  CC_MD5_Init(ctx);
  return 0;
}
inline int mbedtls_md5_update(mbedtls_md5_context* ctx, const unsigned char* in, size_t len) {
  CC_MD5_Update(ctx, in, (CC_LONG)len);
  return 0;
}
inline int mbedtls_md5_finish(mbedtls_md5_context* ctx, unsigned char out[16]) {
  CC_MD5_Final(out, ctx);
  return 0;
}
