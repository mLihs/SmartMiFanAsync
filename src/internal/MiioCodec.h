#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "mbedtls/aes.h"
#include "mbedtls/md5.h"

namespace MiioCodec {

constexpr uint16_t kMagic = 0x2131;
constexpr uint16_t kPort = 54321;
constexpr size_t kHeaderSize = 32;
constexpr size_t kTokenSize = 16;

inline uint16_t toBe16(uint16_t v) {
  return static_cast<uint16_t>((v >> 8) | (v << 8));
}

inline uint32_t toBe32(uint32_t v) {
  return ((v >> 24) & 0xFFu) | ((v >> 8) & 0xFF00u) |
         ((v << 8) & 0xFF0000u) | ((v << 24) & 0xFF000000u);
}

inline uint16_t fromBe16(const uint8_t* p) {
  return static_cast<uint16_t>((p[0] << 8) | p[1]);
}

inline uint32_t fromBe32(const uint8_t* p) {
  return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

inline void md5(const uint8_t* in, size_t len, uint8_t out[16]) {
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, in, len);
  mbedtls_md5_finish(&ctx, out);
  mbedtls_md5_free(&ctx);
}

inline bool hexToBytes16(const char* hex, uint8_t out16[16]) {
  if (!hex) return false;
  for (int i = 0; i < 16; ++i) {
    const char c1 = hex[i * 2];
    const char c2 = hex[i * 2 + 1];
    auto nib = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
      if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      return -1;
    };
    const int n1 = nib(c1);
    const int n2 = nib(c2);
    if (n1 < 0 || n2 < 0) return false;
    out16[i] = static_cast<uint8_t>((n1 << 4) | n2);
  }
  return true;
}

inline void bytesToHex16(const uint8_t in16[16], char out[33]) {
  static const char* kHex = "0123456789abcdef";
  for (int i = 0; i < 16; ++i) {
    out[i * 2] = kHex[(in16[i] >> 4) & 0x0F];
    out[i * 2 + 1] = kHex[in16[i] & 0x0F];
  }
  out[32] = '\0';
}

inline void computeKeyIv(const uint8_t token[16], uint8_t key[16], uint8_t iv[16]) {
  md5(token, 16, key);
  uint8_t tmp[32];
  memcpy(tmp, key, 16);
  memcpy(tmp + 16, token, 16);
  md5(tmp, 32, iv);
}

inline size_t pkcs7Unpad(uint8_t* buffer, size_t len) {
  if (len == 0) return 0;
  const uint8_t pad = buffer[len - 1];
  if (pad == 0 || pad > 16 || pad > len) return 0;
  for (uint8_t i = 0; i < pad; ++i) {
    if (buffer[len - 1 - i] != pad) return 0;
  }
  return len - pad;
}

// 1.x padding: one NUL byte, then PKCS#7.
inline size_t padPlain(const uint8_t* plain, size_t len, uint8_t* out, size_t outCap) {
  const size_t raw = len + 1;
  const size_t pad = 16 - (raw % 16);
  const size_t total = raw + pad;
  if (total > outCap) return 0;
  memcpy(out, plain, len);
  out[len] = 0x00;
  memset(out + len + 1, static_cast<uint8_t>(pad), pad);
  return total;
}

inline size_t encrypt(const uint8_t key[16], const uint8_t iv0[16],
                      const uint8_t* plain, size_t len,
                      uint8_t* out, size_t outCap) {
  const size_t total = padPlain(plain, len, out, outCap);
  if (total == 0) return 0;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  uint8_t iv[16];
  memcpy(iv, iv0, 16);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, total, iv, out, out);
  mbedtls_aes_free(&aes);
  return total;
}

inline bool decryptInPlace(const uint8_t key[16], const uint8_t iv0[16],
                           uint8_t* data, size_t len, size_t& plainLen) {
  if (len == 0 || (len % 16) != 0) return false;
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, key, 128);
  uint8_t iv[16];
  memcpy(iv, iv0, 16);
  const int rc = mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, len, iv, data, data);
  mbedtls_aes_free(&aes);
  if (rc != 0) return false;
  plainLen = pkcs7Unpad(data, len);
  return plainLen != 0;
}

inline void writeChecksum(uint8_t header[32], const uint8_t token[16],
                          const uint8_t* cipher, size_t clen) {
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, header, 16);
  mbedtls_md5_update(&ctx, token, 16);
  if (cipher && clen) mbedtls_md5_update(&ctx, cipher, clen);
  mbedtls_md5_finish(&ctx, header + 16);
  mbedtls_md5_free(&ctx);
}

inline bool verifyChecksum(const uint8_t* packet, size_t len, const uint8_t token[16]) {
  if (len < kHeaderSize) return false;
  uint8_t expect[16];
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  mbedtls_md5_starts(&ctx);
  mbedtls_md5_update(&ctx, packet, 16);
  mbedtls_md5_update(&ctx, token, 16);
  if (len > kHeaderSize) mbedtls_md5_update(&ctx, packet + kHeaderSize, len - kHeaderSize);
  mbedtls_md5_finish(&ctx, expect);
  mbedtls_md5_free(&ctx);
  return memcmp(expect, packet + 16, 16) == 0;
}

inline void buildHeader(uint8_t header[32], uint16_t totalLen,
                        const uint8_t deviceId[4], uint32_t ts) {
  memset(header, 0, 32);
  header[0] = 0x21;
  header[1] = 0x31;
  header[2] = static_cast<uint8_t>(totalLen >> 8);
  header[3] = static_cast<uint8_t>(totalLen);
  memcpy(header + 8, deviceId, 4);
  header[12] = static_cast<uint8_t>(ts >> 24);
  header[13] = static_cast<uint8_t>(ts >> 16);
  header[14] = static_cast<uint8_t>(ts >> 8);
  header[15] = static_cast<uint8_t>(ts);
}

inline bool parseHeader(const uint8_t* packet, size_t len, uint32_t& ts) {
  if (len < kHeaderSize) return false;
  if (fromBe16(packet) != kMagic) return false;
  if (fromBe16(packet + 2) != len) return false;
  ts = fromBe32(packet + 12);
  return true;
}

inline bool isHelloReply(const uint8_t* rx, size_t len) {
  return len == 32 && rx[0] == 0x21 && rx[1] == 0x31 && rx[2] == 0x00 && rx[3] == 0x20;
}

inline bool jsonExtractString(const char* json, const char* key, char* out, size_t outLen) {
  if (!json || !key || !out || outLen == 0) return false;
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
  const char* start = strstr(json, pattern);
  if (!start) return false;
  start += strlen(pattern);
  const char* end = strchr(start, '"');
  if (!end) return false;
  size_t n = static_cast<size_t>(end - start);
  if (n >= outLen) n = outLen - 1;
  memcpy(out, start, n);
  out[n] = '\0';
  return true;
}

inline uint32_t jsonExtractUint(const char* json, const char* key) {
  if (!json || !key) return 0;
  char asString[24];
  if (jsonExtractString(json, key, asString, sizeof(asString))) {
    return static_cast<uint32_t>(strtoul(asString, nullptr, 10));
  }
  char pattern[32];
  snprintf(pattern, sizeof(pattern), "\"%s\":", key);
  const char* start = strstr(json, pattern);
  if (!start) return 0;
  start += strlen(pattern);
  while (*start == ' ' || *start == '\t') ++start;
  char buffer[24];
  size_t n = 0;
  while (start[n] && start[n] != ',' && start[n] != '}' && n < sizeof(buffer) - 1) {
    buffer[n] = start[n];
    ++n;
  }
  while (n > 0 && (buffer[n - 1] == ' ' || buffer[n - 1] == '\t')) --n;
  buffer[n] = '\0';
  return static_cast<uint32_t>(strtoul(buffer, nullptr, 10));
}

inline bool jsonHasError(const char* json) {
  if (!json) return false;
  if (strstr(json, "\"error\"")) return true;
  const char* code = strstr(json, "\"code\":");
  if (!code) return false;
  code += 7;
  while (*code == ' ' || *code == '\t') ++code;
  return *code && *code != '0';
}

inline uint32_t didFromDeviceId(const uint8_t deviceId[4]) {
  return (static_cast<uint32_t>(deviceId[0]) << 24) |
         (static_cast<uint32_t>(deviceId[1]) << 16) |
         (static_cast<uint32_t>(deviceId[2]) << 8) |
         static_cast<uint32_t>(deviceId[3]);
}

}  // namespace MiioCodec
