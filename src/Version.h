// =============================================================================
//  MeteoPlaneRadar
//  Firmware version - THE single place to bump on every release.
//
//  Why a header and not the .ino: Arduino compiles every .cpp as its own
//  translation unit, so a #define living in the .ino is invisible to
//  ScreenSettings.cpp / OTA.cpp. Putting it here lets every module show the
//  same string.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once

// Bump this on every release (shown on the Settings screen, the OTA screen and
// in the serial banner). Describe the change in CHANGELOG.md (repo root) -
// that file is the single source of truth for the version history.
#define FW_VERSION "0.7.0"
