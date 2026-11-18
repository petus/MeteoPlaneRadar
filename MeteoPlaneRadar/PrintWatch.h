// =============================================================================
//  MeteoPlaneRadar
//  3D printer watch (0.9.0) - Prusa through PrusaLink, Klipper through Moonraker.
//
//  A task of its own on the other core asks the printer how it is doing, keeps
//  the answer in a snapshot and a short temperature history, and turns state
//  changes into events (print finished, print failed, print paused). The rest
//  of the firmware only ever reads copies - PrintWatch_Get() - so a printer
//  that is switched off, slow or half-broken cannot hold up the display, the
//  web server or the other downloads. Every request has short timeouts and the
//  task asks rarely while nobody answers (see PRINT_POLL_* in Config.h).
//
//  Read-only: the device never sends a command to the printer.
//
//  Built for one printer; the state lives in one struct so a second one later
//  is a matter of an array, not of a redesign.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>
#include <math.h>      // NAN

enum PrintState : uint8_t {
  PS_DISABLED = 0,   // watch off or no host set
  PS_UNKNOWN,        // configured, no answer yet
  PS_OFFLINE,        // nobody answers (printer off, wrong address)
  PS_AUTH,           // the printer wants a password / API key we do not have
  PS_BADREPLY,       // answers, but not like PrusaLink / Moonraker
  PS_IDLE,           // on, not printing
  PS_PREPARE,        // heating, busy
  PS_PRINTING,
  PS_PAUSED,         // paused, or the printer needs attention (Prusa)
  PS_FINISHED,
  PS_FAILED,         // error
  PS_CANCELLED,      // stopped by the user
};

// Is a job going on (the ring and the times mean something)?
inline bool PrintState_Active(PrintState s) {
  return s == PS_PRINTING || s == PS_PAUSED || s == PS_PREPARE;
}
// Did the printer answer at all?
inline bool PrintState_Online(PrintState s) { return s >= PS_IDLE; }

struct PrintInfo {
  PrintState state = PS_DISABLED;
  float    pct = -1;             // 0..100 by file position, -1 = unknown
  int32_t  elapsedS = -1;        // how long it has been printing
  int32_t  remainS = -1;         // how long it still needs
  float    tNoz = NAN, tNozT = NAN, tBed = NAN, tBedT = NAN;   // actual / target
  int16_t  layer = -1, layers = -1;
  char     file[48] = "";        // job name, UTF-8, sanitised
  char     material[32] = "";    // from the file metadata, if the slicer wrote it
  float    filamentG = -1;       // filament for the whole job, grams
  char     message[64] = "";     // why it failed / wants attention
  uint32_t updatedMs = 0;        // millis() of the last answer
  uint32_t nextPollMs = 0;       // millis() of the next attempt
  uint16_t fails = 0;            // failed polls in a row
  uint8_t  type = 0;             // PRINT_TYPE_*
};

enum PrintEvent : uint8_t { PE_NONE = 0, PE_FINISHED, PE_FAILED, PE_PAUSED };

void PrintWatch_Begin();                  // setup(): starts the task
void PrintWatch_Tick(bool screenShown);   // loop(): hands over config changes
bool PrintWatch_Get(PrintInfo* out);      // copy of the latest snapshot
// The next event, once. msg gets the printer's own words (error text), or "".
PrintEvent PrintWatch_TakeEvent(char* msg, size_t cap);
// The temperature history, oldest first, in tenths of a degree (INT16_MIN =
// no value). ageS[i] is how many seconds ago sample i was taken. Returns n.
int  PrintWatch_History(int16_t* noz, int16_t* nozT, int16_t* bed, int16_t* bedT,
                        uint16_t* ageS, int max);
// "tiskne 47 %", "nedostupna" ... for the web status table (ASCII, Czech).
void PrintWatch_StatusText(char* out, size_t cap);
// Short machine name of the state for the web page and Home Assistant.
const char* PrintState_Name(PrintState s);
