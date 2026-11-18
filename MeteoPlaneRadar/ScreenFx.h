// =============================================================================
//  MeteoPlaneRadar
//  Screen: exchange rates (0.9.6). Up to six currencies, each as
//  "100 HUF   6,123  ▲0,12%": what that amount costs in the home currency and
//  how it moved against the previous fixing. Title: the home currency; under
//  it the source (CNB or ECB) and the date of the fixing.
//
//  Off by default; switched on in the web UI, where the home currency and the
//  list are chosen too. No controls on the glass.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenFx_Enter();
bool ScreenFx_Tick();       // true = redraw
void ScreenFx_Draw();
