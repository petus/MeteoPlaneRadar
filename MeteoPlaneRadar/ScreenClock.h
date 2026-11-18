// =============================================================================
//  MeteoPlaneRadar
//  Screen: clock - interface.
//
//  A large digital clock with the date, the current temperature and a seconds
//  ring around the rim. Everything it shows comes from sources the device polls
//  anyway (the HTTP Date header for the time, Open-Meteo for the weather), so
//  the screen adds no new dependency - and specifically no Home Assistant.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <time.h>
#include <Arduino.h>

void ScreenClock_Enter();
void ScreenClock_Draw();
bool ScreenClock_Tick();                  // true = needs a redraw
bool ScreenClock_HandleTap(int x, int y); // toggles night mode when auto is off
// 0.9.0: a small coloured dot at the bottom - a print alert nobody answered.
// 0 = none. Cleared by the next touch.
void ScreenClock_SetBadge(uint16_t color);

// textcheck only (0.9.2): show the name day of this date instead of today's.
// 0 = today.
void ScreenClock_TestNameDate(int month, int day);

// tools/textcheck only (0.9.7): draw the Moon's phase as at t; 0 = now.
void ScreenClock_TestMoonTime(time_t t);
