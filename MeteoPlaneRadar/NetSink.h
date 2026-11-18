// =============================================================================
//  MeteoPlaneRadar
//  Two ways of reading an HTTP body, both over HTTPClient::writeToStream().
//
//  Why this exists
//  ---------------
//  Every body used to be read by hand off http.getStreamPtr(). That pointer is
//  the RAW socket: it does NOT strip Transfer-Encoding: chunked. When adsb.fi
//  moved behind a proxy that answers chunked, the hex block sizes stayed in the
//  buffer, so the body began "2f8a\r\n{"ac":[" - ArduinoJson read that leading
//  hex as a number, returned Ok, and the document had no "ac" at all. Nothing
//  logged an error, because on its own terms nothing had failed.
//
//  writeToStream() decodes chunked correctly but needs somewhere to put the
//  bytes, and getString() would put the whole body on the heap that TLS draws
//  from. The two sinks doing the work live in NetSink.cpp; nothing outside needs
//  to see them. They also carry the guards writeToStream() has not got: a
//  capacity limit that is reported instead of silently truncating, and a
//  wall-clock budget, because writeToStreamDataBlock() spins on delay(1) with no
//  timeout of its own.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

class HTTPClient;

// Read an already-issued GET into buf. Returns the byte count, or -1 on any
// failure - all of them logged with the given tag. The body is left
// NUL-terminated. For bodies that have to be kept whole: JSON, PNG, tiles.
long Net_ReadBody(HTTPClient& http, uint8_t* buf, size_t cap, const char* tag,
                  void (*poll)() = nullptr);

// Longest token a scan callback may look for. Everything shorter is guaranteed
// to land whole in at least one window.
#define NET_SCAN_MAX_TOKEN 64

// Scan an already-issued GET without storing it - for bodies that are searched
// rather than kept. The CHMU directory listing is the case that needs it: it has
// no useful upper bound, so any buffer reserved for it is a ceiling waiting to
// be hit. cb gets a NUL-terminated window carrying the tail of the previous one,
// so a token shorter than NET_SCAN_MAX_TOKEN always arrives whole - and, because
// the windows overlap, may arrive twice. Callbacks have to tolerate that.
// Returns the byte count, or -1 on failure.
typedef void (*NetScanFn)(const char* window, void* user);
long Net_ScanBody(HTTPClient& http, NetScanFn cb, void* user, const char* tag,
                  void (*poll)() = nullptr);

// True when there is enough INTERNAL RAM for a TLS handshake (roughly 45 kB;
// PSRAM cannot be used for it). Starting one with less fails deep inside
// mbedTLS and surfaces as a bare "HTTP -1", so callers check first and skip the
// poll, leaving the previous data on screen.
bool Net_HeapOk(const char* tag);

// 0.9.2: the tag of the body being read right now (nullptr between reads). The
// stuck-loop guard (Watchdog_PollFeed) writes it down before it lets the
// watchdog restart the board, so the next boot can say what hung.
void        Net_SetActivity(const char* what);
const char* Net_Activity();
