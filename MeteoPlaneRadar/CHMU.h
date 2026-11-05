// MeteoPlaneRadar - vyvoj / development: chiptron.cz
// =============================================================================
//  MeteoPlaneRadar - meteoradar CHMU: stahovani srazkoveho kompozitu.
//  Rozhrani + kalibrace geografickych okraju + animace (vice ramcu).
// =============================================================================
#pragma once
#include <Arduino.h>

#define CHMU_INDEX_URL        "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/"
// 0.8.0: the same composite with the echoes that do not reach the ground drawn
// paler (CHMU checks them against PseudoCAPPI 2 km). Same file names, same
// geometry - only the directory differs. See RADAR_SRC_CHMU_MASKED in Config.h.
#define CHMU_INDEX_URL_MASKED "https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png_masked/"

// Geograficke ohraniceni CELEHO obrazku PNG (dle dokumentace CHMU).
//   z.d. 11,267 - 20,770 ; z.s. 48,047 - 52,167
#define CHMU_LON_LEFT   11.267f
#define CHMU_LON_RIGHT  20.770f
#define CHMU_LAT_TOP    52.167f
#define CHMU_LAT_BOTTOM 48.047f

// Ohraniceni skutecnych DAT (uzsi nez cely obrazek). Za timto rozsahem je v PNG
// vlevo? ne - nahore titulek "CZRAD - Z: MAX ..." a vpravo barevna skala/okraj.
// Tyto oblasti maskujeme na cerno, aby se nezobrazovaly jako srazky.
#define CHMU_LON_DATA_RIGHT 19.624f
#define CHMU_LAT_DATA_TOP   51.458f

#define CHMU_MAX_PNG 131072      // max velikost jednoho PNG (~15-30 kB, rezerva)
#define CHMU_ANIM_MAX 6          // max poctu ramcu animace

void        CHMU_SetPollFn(void (*fn)());

// Which of the two directories the animation comes from. Set from the radar
// source setting before the first fetch.
void        CHMU_SetMasked(bool masked);
bool        CHMU_Masked();

// The newest single frame, into the caller's buffer - for the storm watch,
// which needs one picture every five minutes and not the whole animation.
// name receives the file name (it carries the UTC time). Returns true on a
// complete, valid PNG.
bool        CHMU_FetchLatest(bool masked, uint8_t* buf, size_t cap, size_t* outSize, String* name);
String      CHMU_AnimName(int i);       // file name of animation frame i
String      CHMU_TimeTextFromName(const String& name);   // HH:MM local

// --- Animace: nejnovejsich wantN ramcu (5 min krok) ---
// Vraci pocet stazenych ramcu. Ramce jsou serazene 0 = nejstarsi ... N-1 = nyni.
int         CHMU_FetchAnim(int wantN);
int         CHMU_AnimCount();
uint8_t*    CHMU_AnimData(int i);
size_t      CHMU_AnimSize(int i);
String      CHMU_AnimTimeText(int i);   // HH:MM (lokalni cas snimku)
