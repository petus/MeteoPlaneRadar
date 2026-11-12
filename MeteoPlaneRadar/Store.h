// =============================================================================
//  MeteoPlaneRadar
//  Card store - a signed list of cards the device can install by itself.
//
//  The store is two static files next to the cards (cards/index.json and
//  cards/index.json.sig, made by tools/make_index.py) on any web server;
//  STORE_INDEX_URL in Config.h says where. The device downloads the index,
//  checks its signature against the key in StoreKey.h, and offers the cards
//  in the web UI. Installing one downloads the card file, checks it against
//  the SHA-256 the index gave for it, and hands it to Cards_Install() - the
//  same validation an uploaded file goes through.
//
//  What the signature is for: the firmware does not check TLS certificates,
//  so without it anyone on the path could offer cards of their own. What it
//  is not: a review. It says the index came from the holder of the key and
//  nothing about what a card does. Cards are still data, not code.
//
//  An index carries a serial number. The device remembers the highest it has
//  accepted and refuses an older one, so a correctly signed but outdated index
//  cannot be replayed to bring back a card that was withdrawn.
//
//  Uploading a card by hand (Cards.h) works as before and needs no signature.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Downloads and verifies the index. On success `out` is
//   { "serial": N, "cards": [ { id, title, host, size, desc: {cs, en}, script, installed } ] }
// On failure err holds a short English reason for the web UI.
bool Store_List(JsonDocument& out, char* err, size_t errCap);

// Installs (or replaces) the card with this id from the store.
bool Store_Install(const char* id, char* err, size_t errCap);
