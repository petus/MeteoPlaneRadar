// =============================================================================
//  MeteoPlaneRadar
//  Layout - shared screen bands and a collision check, so no two elements can
//  ever end up drawn on top of each other.
//
//  The round panel has little usable height at top and bottom, and five screens
//  compete for it. Tuning coordinates by hand looks fine until someone enables
//  one more thing, so there are two mechanisms instead:
//
//  1) FIXED BANDS (the LY_* constants). Every screen puts the same kind of
//     information on the same line. The clock is always at LY_STATUS, the range
//     always at LY_RANGE. Two screens cannot disagree about where something
//     goes, and the self-test below proves the bands themselves do not overlap.
//
//  2) RUNTIME CLAIMS. Chrome is RESERVED before the map is drawn. Anything
//     positioned by data - city labels, aircraft callsigns - must then CLAIM
//     its rectangle and is dropped if the space is taken. Priority is draw
//     order: half a callsign under the legend is worse than no callsign.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>
#include "Display_ST7701.h"

// --- Fixed bands (y of the TOP of the text, matching Arduino_GFX cursors) ----
// Top stack, from the rim down. The gaps are deliberate: size-2 text is 16 px
// tall, size-1 is 8 px, and the self-test checks these do not run into each
// other.
#define LY_DOTS        18    // screen selector dots (this one is a CENTRE y)
#define LY_STATUS      30    // clock + outside temperature      (size 2)
#define LY_SUB         52    // aircraft count / frame dots      (size 1)
#define LY_LEGEND      74    // altitude legend / frame time     (size 1)
#define LY_NOTE        86    // small note under the legend      (size 1)

// Bottom stack, from the rim up.
#define LY_RANGE       (LCD_HEIGHT - 76)   // range readout       (size 2)
#define LY_RANGE_DOTS  (LCD_HEIGHT - 50)   // range dots (CENTRE y)
#define LY_FOOTER      418                 // signature           (size 2)

// The seconds ring on the clock screen lives outside everything else.
#define LY_SEC_RING_R  232

// Text metrics for the built-in GFX font.
#define LY_CHAR_W(size) (6 * (size))
#define LY_CHAR_H(size) (8 * (size))

// Start a frame - drops every reservation. Call once, before drawing anything.
void Layout_Begin();

// Chrome: take the space whether or not something is already there. Chrome is
// positioned by design and always wins; the ordering rule is that it is
// reserved before any data-positioned element gets a chance to claim.
void Layout_Reserve(int x, int y, int w, int h);

// Reserve a full-width horizontal band (the whole chord of the circle at that
// height). Use for anything centred that spans the screen.
void Layout_ReserveBand(int y, int h);

// Reserve exactly what a centred string will occupy.
void Layout_ReserveTextCentered(const char* s, uint8_t size, int cx, int y);

// Data-positioned element: take the space only if it is free.
// Returns false when it is not - the caller then draws nothing at all.
bool Layout_Claim(int x, int y, int w, int h);

// Does the rectangle fit on the round glass? Layout_Claim() refuses anything
// that does not (0.8.6).
bool Layout_InsideCircle(int x, int y, int w, int h);

// Width of a string in the built-in font.
int  Layout_TextW(const char* s, uint8_t size);

// Half the width of the display circle at height y - how much room a line of
// text actually has there. On a round panel the usable width shrinks fast
// towards the rim, so this has to be measured, not assumed.
int  Layout_ChordHalf(int y);

// Walk the fixed bands and report any overlap on the serial line. Called once
// at boot when LAYOUT_DEBUG is on - catches a mistyped constant before it turns
// into two labels sitting on each other.
void Layout_SelfTest();

// Text collision check (TEXT_CHECK in Config.h, 0.8.6). NoteText records one
// drawn string; TextCheckFrame compares everything recorded since the last
// call, reports overlaps and texts outside the circle, and starts over.
// Returns the number of problems found. Both are empty when TEXT_CHECK is 0.
void Layout_NoteText(int x, int y, int w, int h, const char* s, size_t n);
int  Layout_TextCheckFrame();
