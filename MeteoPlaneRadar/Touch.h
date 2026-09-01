// =============================================================================
//  MeteoPlaneRadar
//  Touch facade - one interface, two controllers underneath.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Boards:  2.1 -> CST820 (Touch_CST820.cpp), 2.8C -> GT911 (Touch_GT911.cpp)
//
//  The rest of the sketch only ever sees this header. Which driver is behind it
//  is decided once at boot by Board_Detect(); nothing above this line cares,
//  because a tap is a tap.
// =============================================================================
#pragma once
#include <Arduino.h>

struct TouchData {
  uint16_t x = 0;
  uint16_t y = 0;
  uint8_t  points = 0;   // 0 = no touch
};

// Initialise the controller the detected board actually has. Returns false if
// it does not answer - not fatal, the screens keep running unattended.
bool Touch_Init();

// One sample. Cheap when nothing is happening: both drivers only go to the bus
// when the controller's INT says there is something to fetch, when a finger is
// already down, or when the slow fallback poll comes round.
void Touch_Read(TouchData* out);
