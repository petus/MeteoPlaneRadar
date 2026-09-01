// =============================================================================
//  MeteoPlaneRadar
//  Touch facade - hands every call to whichever controller the board has.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//
//  A function pointer, not a branch per sample: Touch_Read() is called from
//  netPoll(), which runs thousands of times a second inside a download, and the
//  board cannot change while the thing is running.
// =============================================================================
#include "Touch.h"
#include "Board.h"
#include "Touch_CST820.h"
#include "Touch_GT911.h"

static void (*s_read)(TouchData*) = nullptr;

bool Touch_Init() {
  if (Board_Model() == BOARD_LCD_2_8C) {
    s_read = GT911_Read;
    return GT911_Init();
  }
  s_read = CST820_Read;
  return CST820_Init();
}

void Touch_Read(TouchData* out) {
  if (!s_read) { out->points = 0; return; }   // Touch_Init() failed or not run
  s_read(out);
}
