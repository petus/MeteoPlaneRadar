// =============================================================================
//  MeteoPlaneRadar
//  One place for "fetch this URL over TLS and hand me the body".
//
//  Every endpoint in this project needs the same four things: a check that
//  there is enough internal RAM for the handshake, an insecure TLS client
//  (we do not pin certificates - see the note in Outside.h), the response as a
//  complete string rather than a stream, and the "Date" header fed to the clock
//  on the way past. Repeating that in five modules is how they drift apart.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

// GET the URL and put the whole body in `out`.
//   tag - short name for the serial log ("PREDPOVED", "RAINVIEWER", ...)
// Returns false on no link, not enough heap, a non-200 status or an empty body;
// the reason is printed once, so a caller can just retry later.
bool Net_GetString(const char* url, String& out, const char* tag);
// 0.9.7: HTTP status of the last Net_GetString() (429 = too many requests),
// negative for a connection error.
int  Net_LastCode();

// GET the URL into a caller-supplied buffer - for binary payloads (PNG tiles)
// where a String would double the memory needed.
// On success *outLen holds the number of bytes written.
bool Net_GetBinary(const char* url, uint8_t* buf, size_t cap, size_t* outLen,
                   const char* tag);

// HEAD only, purely to read the "Date" header and keep the clock seeded. Used
// when the clock screen is the only one enabled and nothing else fetches.
bool Net_TouchDate(const char* url);

// --- Keep-alive session -----------------------------------------------------
//
// A TLS handshake needs ~45 kB of internal RAM, which the device does not
// reliably have while the web server is running - so a burst of one handshake
// per radar tile ended up with tiles refused by the heap guard and drawn black.
// Between Begin and End, Net_GetBinary reuses a single connection. Always pair
// them: an open session holds its buffers until closed.
void Net_SessionBegin();
void Net_SessionEnd();

// Called from loop() while a long transfer runs, so the watchdog stays fed.
void Net_SetPollFn(void (*fn)());
