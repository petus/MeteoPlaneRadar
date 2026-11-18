// =============================================================================
//  MeteoPlaneRadar
//  3D printer watch (0.9.0): what the printers say, turned into a PrintInfo.
//
//  Pure functions - no network, no task, no globals - so they can be tested on
//  a PC against recorded answers (tools/printcheck). PrintWatch.cpp does the
//  asking and calls these with the parsed JSON.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <ArduinoJson.h>
#include "PrintWatch.h"

// --- PrusaLink (MK4, MK3.9, Mini, XL, Core One) ------------------------------
// GET /api/v1/status. jobId gets the running job's id, -1 when there is none.
void PrintParse_PrusaStatus(JsonObjectConst root, PrintInfo& io, int32_t* jobId);
// GET /api/v1/job - only for what /status lacks: name, material, grams.
void PrintParse_PrusaJob(JsonObjectConst root, PrintInfo& io);

// --- Moonraker (Klipper) -----------------------------------------------------
// GET /printer/objects/query?print_stats&virtual_sdcard&display_status&extruder&heater_bed
// file gets print_stats.filename (to fetch its metadata when it changes);
// printDur the time spent actually printing (pauses excluded), progress 0..1.
void PrintParse_Moonraker(JsonObjectConst root, PrintInfo& io,
                          char* file, size_t fileCap, double* printDur, double* progress);
// GET /server/files/metadata?filename=... estTime: the slicer's estimate, s.
void PrintParse_MoonrakerMeta(JsonObjectConst root, PrintInfo& io, double* estTime);
// Remaining time the way Mainsail/Fluidd estimate it: from file progress once
// there is enough of it, from the slicer estimate before that.
int32_t PrintParse_MoonrakerRemain(double printDur, double progress, double estTime);

// --- Shared -------------------------------------------------------------------
// UTF-8-safe copy: drops control characters and broken sequences, never cuts
// a character in half, and takes ".gcode" / ".bgcode" / ".3mf" off the end.
void PrintParse_CleanName(char* dst, size_t cap, const char* src, bool stripExt);
// The event a state change means, if any. Cancelling is not an event: it is
// almost always the user's own doing.
PrintEvent PrintParse_Event(PrintState from, PrintState to);

// --- HTTP Digest (PrusaLink user + password) ---------------------------------
struct DigestChallenge {
  char realm[64] = "";
  char nonce[96] = "";
  char opaque[64] = "";
  bool qopAuth = false;
};
void Md5Hex(const char* s, size_t n, char out[33]);
bool Digest_Parse(const char* wwwAuthenticate, DigestChallenge& c);
// The value of the Authorization header for one request.
void Digest_Header(const DigestChallenge& c, const char* method, const char* uri,
                   const char* user, const char* pass, uint32_t nc, const char* cnonce,
                   char* out, size_t cap);
