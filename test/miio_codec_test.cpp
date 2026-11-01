#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "mbedtls/md5.h"
#include "mbedtls/aes.h"
#include "../src/internal/MiioCodec.h"

static int g_fails = 0;

static void expect(bool ok, const char* name) {
  if (!ok) {
    printf("FAIL %s\n", name);
    g_fails++;
  } else {
    printf("OK   %s\n", name);
  }
}

int main() {
  uint8_t tok[16];
  expect(MiioCodec::hexToBytes16("00112233445566778899aabbccddeeff", tok), "hex valid");
  expect(tok[0] == 0x00 && tok[15] == 0xff, "hex bytes");
  expect(!MiioCodec::hexToBytes16("00112233445566778899aabbccddeeGG", tok), "hex invalid");

  char hex[33];
  MiioCodec::bytesToHex16(tok, hex);
  expect(strcmp(hex, "00112233445566778899aabbccddeeff") == 0, "hex roundtrip");

  uint8_t key[16], iv[16];
  MiioCodec::computeKeyIv(tok, key, iv);
  expect((key[0] | key[15] | iv[0] | iv[15]) != 0, "key/iv nonzero");

  const char* json = "{\"id\":7,\"method\":\"miIO.info\"}";
  uint8_t cipher[128];
  const size_t clen = MiioCodec::encrypt(key, iv, reinterpret_cast<const uint8_t*>(json),
                                         strlen(json), cipher, sizeof(cipher));
  expect(clen > 0 && (clen % 16) == 0, "encrypt size");
  uint8_t plain[128];
  memcpy(plain, cipher, clen);
  size_t plen = 0;
  expect(MiioCodec::decryptInPlace(key, iv, plain, clen, plen), "decrypt");
  expect(plen >= strlen(json) && memcmp(plain, json, strlen(json)) == 0, "decrypt payload");

  uint8_t header[32];
  uint8_t did[4] = {1, 2, 3, 4};
  MiioCodec::buildHeader(header, static_cast<uint16_t>(32 + clen), did, 0x11223344);
  MiioCodec::writeChecksum(header, tok, cipher, clen);
  uint8_t packet[32 + 128];
  memcpy(packet, header, 32);
  memcpy(packet + 32, cipher, clen);
  expect(MiioCodec::verifyChecksum(packet, 32 + clen, tok), "checksum");
  uint32_t ts = 0;
  expect(MiioCodec::parseHeader(packet, 32 + clen, ts) && ts == 0x11223344, "header ts");

  uint8_t hello[32];
  memset(hello, 0xFF, 32);
  hello[0] = 0x21;
  hello[1] = 0x31;
  hello[2] = 0x00;
  hello[3] = 0x20;
  expect(MiioCodec::isHelloReply(hello, 32), "hello reply");
  expect(!MiioCodec::isHelloReply(hello, 16), "hello short");

  const char* info = "{\"id\":9,\"result\":{\"model\":\"dmaker.fan.p5\",\"fw_ver\":\"1.2.3\",\"hw_ver\":\"ESP32\"}}";
  char model[24] = {};
  expect(MiioCodec::jsonExtractString(info, "model", model, sizeof(model)), "json model");
  expect(strcmp(model, "dmaker.fan.p5") == 0, "json model value");
  expect(MiioCodec::jsonExtractUint(info, "id") == 9, "json id");
  expect(!MiioCodec::jsonHasError(info), "json no error");
  expect(MiioCodec::jsonHasError("{\"id\":1,\"error\":{\"code\":-9999}}"), "json error");
  expect(MiioCodec::jsonHasError("{\"id\":1,\"code\":1}"), "json code");

  uint8_t padBuf[32];
  memset(padBuf, 0xAB, 16);
  memset(padBuf + 16, 0x10, 16);
  expect(MiioCodec::pkcs7Unpad(padBuf, 32) == 16, "pkcs7 16");
  memset(padBuf, 0x03, 8);
  expect(MiioCodec::pkcs7Unpad(padBuf, 8) == 0, "pkcs7 bad");

  printf(g_fails ? "\n%d FAILED\n" : "\nALL PASSED\n", g_fails);
  return g_fails ? 1 : 0;
}
