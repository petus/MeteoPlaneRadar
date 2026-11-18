// =============================================================================
//  MeteoPlaneRadar
//  How a data source is doing (0.9.7): when it last answered, how many times
//  in a row it has not, and when the next attempt is due.
//
//  The fetchers already give up on a silent server (Net.cpp: connect, TLS,
//  header and body limits), so a download never hangs. What used to hang was
//  the screen: with no data yet and the source down it said "Loading..." for
//  ever. With this a screen can say "source unavailable, next try in 4 min"
//  instead, and a screen that does have data can tell when it is getting old.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

struct SrcHealth {
  unsigned long okAt   = 0;     // millis() of the last success
  unsigned long nextAt = 0;     // millis() of the next attempt
  uint16_t      fails  = 0;     // failures in a row since the last success
  bool          everOk = false;
  bool          tried  = false;

  // After an attempt: what happened and how long until the next one.
  void result(bool ok, unsigned long waitMs) {
    const unsigned long now = millis();
    tried = true;
    if (ok) { okAt = now; fails = 0; everOk = true; }
    else if (fails < 9999) fails++;
    nextAt = now + waitMs;
  }
  // A failure whose wait grows: base after the first, doubled each time, cap.
  void failed(unsigned long base, unsigned long cap) {
    result(false, 0);
    nextAt = millis() + backoff(base, cap);
  }
  // Settings changed, start over ("Loading..." again, not "unavailable").
  void reset() { fails = 0; tried = false; everOk = false; nextAt = 0; }

  bool down() const { return fails > 0; }            // the last attempt failed
  // Milliseconds since the last success; ~0UL when there never was one.
  unsigned long age() const { return everOk ? millis() - okAt : ~0UL; }
  // Milliseconds to the next attempt, 0 when it is due.
  unsigned long nextIn() const {
    const long d = (long)(nextAt - millis());
    return d > 0 ? (unsigned long)d : 0;
  }
  // The wait after a failure: base, doubled with each one in a row, at most cap.
  unsigned long backoff(unsigned long base, unsigned long cap) const {
    unsigned long w = base;
    for (uint16_t i = 1; i < fails && w < cap; i++) w *= 2;
    return w < cap ? w : cap;
  }
};
