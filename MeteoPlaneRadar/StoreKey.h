// =============================================================================
//  MeteoPlaneRadar
//  Card store - the public key the store index is checked against.
//
//  This is the PUBLIC half of an ECDSA P-256 key pair. The private half signs
//  cards/index.json (tools/make_index.py) and is not in the repository. A
//  device accepts an index only if its signature matches this key, so whoever
//  builds a firmware decides whose store that firmware trusts: to run your own
//  store, make a key pair, put its public key here and point STORE_INDEX_URL
//  (Config.h) at your index.
// =============================================================================
#pragma once

static const char STORE_PUBKEY_PEM[] =
  "-----BEGIN PUBLIC KEY-----\n"
  "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEHT5GCGcak8wti+TgO0Q6nPFBhqMa\n"
  "pdDcI2w1tGq8z3n9mKD+MdCE8Pta3r8FFhrY0KhDgpmD1N+zvV6icUChcQ==\n"
  "-----END PUBLIC KEY-----\n";
