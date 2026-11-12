// =============================================================================
//  MeteoPlaneRadar
//  Card store - the checks that decide whether something from the store is
//  what its publisher put there.
//
//  The firmware talks HTTPS without checking certificates (Net.cpp), so the
//  transport proves nothing about who answered. These two checks do instead:
//
//    - the index carries an ECDSA P-256 / SHA-256 signature, checked against
//      the public key compiled into the firmware (StoreKey.h);
//    - the index lists the SHA-256 of every card file, so a card is only as
//      trustworthy as the index that named it.
//
//  mbedTLS does the arithmetic - it is in the ESP32 core already (TLS uses
//  it), so this adds no library. Nothing here touches Arduino.h; the file is
//  tested on a PC (tools/sim/storetest.cpp).
// =============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <mbedtls/version.h>
#include <mbedtls/base64.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

static inline bool StoreVerify_Sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
#if MBEDTLS_VERSION_MAJOR >= 3
  return mbedtls_sha256(data, len, out, 0) == 0;
#else
  return mbedtls_sha256_ret(data, len, out, 0) == 0;
#endif
}

// Does the data hash to this hex string (64 digits, either case)?
static inline bool StoreVerify_Sha256Hex(const uint8_t* data, size_t len, const char* wantHex) {
  if (!wantHex || strlen(wantHex) != 64) return false;
  uint8_t h[32];
  if (!StoreVerify_Sha256(data, len, h)) return false;
  static const char DIGITS[] = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    char a = wantHex[2 * i], b = wantHex[2 * i + 1];
    if (a >= 'A' && a <= 'F') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'F') b = (char)(b - 'A' + 'a');
    if (a != DIGITS[h[i] >> 4] || b != DIGITS[h[i] & 0x0F]) return false;
  }
  return true;
}

// Is sigB64 (base64 of a DER ECDSA signature, whitespace allowed) a signature
// of data made with the private half of pubPem?
static inline bool StoreVerify_Signature(const uint8_t* data, size_t len,
                                         const char* sigB64, size_t sigB64Len,
                                         const char* pubPem) {
  if (!data || !sigB64 || !pubPem) return false;

  // A P-256 signature in DER is at most 72 bytes, 96 characters of base64.
  char clean[128];
  size_t n = 0;
  for (size_t i = 0; i < sigB64Len; i++) {
    const char c = sigB64[i];
    if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
    if (n + 1 >= sizeof(clean)) return false;
    clean[n++] = c;
  }
  uint8_t sig[96];
  size_t sigLen = 0;
  if (n == 0 || mbedtls_base64_decode(sig, sizeof(sig), &sigLen,
                                      (const unsigned char*)clean, n) != 0) return false;

  uint8_t hash[32];
  if (!StoreVerify_Sha256(data, len, hash)) return false;

  mbedtls_pk_context pk;
  mbedtls_pk_init(&pk);
  const bool ok =
      mbedtls_pk_parse_public_key(&pk, (const unsigned char*)pubPem, strlen(pubPem) + 1) == 0 &&
      mbedtls_pk_can_do(&pk, MBEDTLS_PK_ECDSA) &&
      mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, sizeof(hash), sig, sigLen) == 0;
  mbedtls_pk_free(&pk);
  return ok;
}

// A card id as Cards.cpp accepts it: 1-23 characters of a-z, 0-9, -.
static inline bool StoreVerify_Id(const char* id) {
  if (!id) return false;
  const size_t n = strlen(id);
  if (n == 0 || n > 23) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

// The only file name an index entry may point at is "<id>.json", next to the
// index. No directories, no other host, nothing to escape from.
static inline bool StoreVerify_FileName(const char* file, const char* id) {
  if (!file || !StoreVerify_Id(id)) return false;
  const size_t n = strlen(id);
  return strncmp(file, id, n) == 0 && strcmp(file + n, ".json") == 0;
}
