// =============================================================================
//  MeteoPlaneRadar
//  Exchange rates (0.9.6): the CNB fixing for a Czech user, the ECB reference
//  rates for everyone else. See the note in Config.h for why two sources and
//  how a cross rate is worked out.
//
//  Fetched only while the Rates screen is switched on - like the energy
//  screens, a device whose owner never asked for rates makes no requests.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"

enum FxSource : uint8_t { FX_SRC_CNB = 0, FX_SRC_ECB = 1 };

// One line of the screen: `rate` units of the home currency for `amount`
// units of `code`, and the same from the previous fixing. NAN when unknown.
struct FxRow {
  char     code[4];
  uint16_t amount;
  float    rate;
  float    prev;
};

// Call from loop(). At most one request per call.
void Fx_Tick();

bool        Fx_Valid();               // there is something to show
int         Fx_Rows(FxRow* out, int max);
FxSource    Fx_Source();
long        Fx_Date();                // fixing date, YYYYMMDD; 0 = unknown
uint32_t    Fx_Gen();                 // changes whenever anything above does
struct SrcHealth;
const SrcHealth& Fx_Health();         // 0.9.7: for "source unavailable"
bool        Fx_Stale();               // data held, but no news for over a day
