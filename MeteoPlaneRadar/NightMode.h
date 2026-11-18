// =============================================================================
//  MeteoPlaneRadar
//  Day / night brightness, driven by the sun or by the clock (0.9.2), and the
//  display switched off for part of the night (0.9.2).
//
//  No light sensor and no home-automation server: sunrise and sunset arrive as
//  part of the forecast request we make anyway, so the automatic switch costs
//  nothing extra. The offset shifts BOTH events symmetrically - a positive
//  value makes the night start earlier and end later, which is what you want in
//  a room that goes dark before the sun is actually down.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

// Call from loop(). Decides day/night and pushes the matching brightness.
void NightMode_Tick();

// Manual switch, used by a short tap on the clock screen when the automatic
// mode is off. Does nothing while automatic mode is on - it would be undone a
// second later anyway.
void NightMode_Toggle();

// Push the brightness for the current state to the panel. Call after the user
// changes a brightness value so the effect is immediate.
void NightMode_Apply();

// --- 0.9.2 ---------------------------------------------------------------------
// Night can also follow a clock window (Settings_NightSource), and the display
// can be switched off altogether for a window of its own (Settings_DarkOn).
//
// Dark means the backlight at 0 - the firmware keeps running, downloads and
// draws as before, so the right picture is there the moment it lights up.
//
// True while the display is switched off right now (dark window, not woken).
// A touch in that state only wakes it and must not do anything else.
bool NightMode_ScreenOff();
// Inside the dark window, woken or not.
bool NightMode_InDarkWindow();
// Light the display for ms (a touch: DARK_WAKE_MS). Outside the dark window it
// does nothing.
void NightMode_Wake(uint32_t ms);
// An alert has the screen. With Settings_DarkAlerts() it lights the display for
// as long as the alert lasts; without, the display stays dark.
void NightMode_SetAlert(bool on);
// The print alert's "full brightness at night" (PRINT_NIGHT_FULL): the day
// level while it is on. NightMode owns the backlight, so this goes through it.
void NightMode_SetBoost(bool on);
