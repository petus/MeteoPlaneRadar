// =============================================================================
//  MeteoPlaneRadar
//  Script cards - a card that draws itself with a small Berry program.
//
//  A JSON card (Cards.h) can show up to four values in a fixed layout. A card
//  with a "script" member gets the whole panel instead: the firmware still
//  fetches the card's address on the card's schedule, and the script decides
//  what to draw from the answer. Berry (berry-lang.github.io, MIT, vendored in
//  src/berry) is the interpreter; it was written for exactly this kind of
//  microcontroller.
//
//  What a script can reach - the whole of it:
//
//    g     drawing: text, lines, rectangles, circles, ring sectors, colours
//    net   the last answer of the card's own address, as Berry maps and lists
//    cfg   the card's "params", the interface language, the time of day
//
//  and Berry's string, math and json modules. There is no file access, no
//  network call a script can make, no way to reach settings or other cards.
//  In particular a script cannot SEND anything: the only address involved is
//  the one written in the card, and the firmware fetches it, not the script.
//
//  That makes a script card a program, which a JSON card is not, so it comes
//  with limits and a way out:
//
//    - scripts are OFF until switched on in the web UI;
//    - one draw may run SCRIPT_TIME_MS and allocate SCRIPT_HEAP_MAX; past
//      either the run is stopped in a way the script cannot catch;
//    - SCRIPT_FAIL_MAX failures in a row switch that card's script off;
//    - SAFE MODE: before any script code runs, its card is noted in memory
//      that survives a reset. If the device restarts while that note is set -
//      a crash, a watchdog reset - the card is switched off at the next boot
//      instead of taking the device down again.
//
//  The remaining risk is the interpreter itself: a bug in it could be reached
//  by a hostile script. Install script cards from sources you would also
//  trust with a firmware update.
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Cards.h"

void Script_Begin();                  // after Cards_Begin(): the switch and the safe-mode check

bool Script_Enabled();                // the global switch, off by default
void Script_SetEnabled(bool on);

// Is this card's script switched off (too many failures, or safe mode)?
// `why` receives the reason that was recorded; may be nullptr.
bool Script_IsOff(const char* id, char* why, size_t whyCap);
void Script_SwitchOn(const char* id); // clear that, e.g. from the web UI

// Draws the card with its script. false = nothing usable was drawn and `why`
// says what to show instead (ASCII, short).
bool Script_Draw(const Card* c, char* why, size_t whyCap);

// Drop the interpreter - call when another card or another screen takes over.
void Script_Release();

// For tests and the status page.
size_t Script_HeapPeak();              // most the interpreter has held, in bytes
size_t Script_HeapUsed();              // what it holds now (0 after Script_Release)
