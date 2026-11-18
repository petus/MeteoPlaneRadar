// =============================================================================
//  MeteoPlaneRadar
//  Cryptocurrency prices (0.9.7) from CoinGecko, in the home currency of the
//  Exchange rates screen, with the change over 24 hours. See Config.h.
//
//  Fetched only while the Crypto screen is switched on.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Config.h"

struct CrRow {
  char   sym[6];
  double price;       // in the home currency; NAN when unknown
  float  chg24;       // per cent over 24 h; NAN when unknown
};

void        Crypto_Tick();            // from loop(), at most one request per call
bool        Crypto_Valid();
int         Crypto_Rows(CrRow* out, int max);
time_t      Crypto_Updated();         // newest CoinGecko timestamp, 0 = unknown
uint32_t    Crypto_Gen();             // changes whenever anything above does
struct SrcHealth;
const SrcHealth& Crypto_Health();
