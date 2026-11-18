// =============================================================================
//  MeteoPlaneRadar
//  Screen: 3D print (0.9.0). A ring of how long it has printed against how long
//  it still needs, the same as numbers in the middle, the material, and the
//  nozzle and bed temperatures of the last half hour.
//
//  Also carries the print alert: a banner over the middle that stays until the
//  glass is touched (or PRINT_ALERT_MAX_MS passes) and blinks unless the user
//  chose a calm night.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#pragma once
#include <Arduino.h>
#include "PrintWatch.h"

void ScreenPrint_Enter();
bool ScreenPrint_Tick();       // true = redraw
void ScreenPrint_Draw();

void ScreenPrint_AlertBegin(PrintEvent e, const char* msg, bool blink);
void ScreenPrint_AlertEnd();
