// =============================================================================
//  MeteoPlaneRadar
//  Screen: cryptocurrencies (0.9.7). Up to six coins from CoinGecko in the
//  home currency, each with the change over 24 hours:
//      "BTC  2,05 mil.  ▼ 1,41%"
//  Off by default; switched on and set up in the web UI (Rates & Crypto tab).
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenCrypto_Enter();
bool ScreenCrypto_Tick();       // true = redraw
void ScreenCrypto_Draw();
