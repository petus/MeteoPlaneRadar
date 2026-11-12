// =============================================================================
//  MeteoPlaneRadar
//  Cards screen - shows the user-installed JSON cards (Cards.h), one at a
//  time.
//
//  To the user every card is a screen of its own: with four built-in screens
//  and two cards switched on, the display cycles through six. The firmware
//  has one screen index for all cards, so the stepping code in the .ino asks
//  this file how many places the screen takes (ScreenCards_Count) and moves
//  inside it (ScreenCards_Step) before it moves on to a neighbouring screen.
//  The swipe that changes the range on the radar screens also steps through
//  the cards, round and round.
// =============================================================================
#pragma once
#include <Arduino.h>

void ScreenCards_Enter();
void ScreenCards_Draw();
bool ScreenCards_Tick();
void ScreenCards_ChangeRange(int dir);

// How many places this screen takes in the cycling, the stepping and the dots:
// one per installed card, and one (the "add a card" hint) when there is none.
int  ScreenCards_Count();
int  ScreenCards_Index();              // which of them is showing, from 0
// To the neighbouring card. false at either end - the caller then leaves for
// the next or the previous screen.
bool ScreenCards_Step(int dir);
// Which card to show when the screen is entered: 0 = the first (coming from
// the screen before), -1 = the last (coming back from the screen after).
void ScreenCards_Select(int which);
// "2/3" for the web UI; empty with fewer than two cards.
void ScreenCards_RangeText(char* out, size_t cap);
// One line of text centred on the round panel at height y (top of the text),
// shrunk from the wanted size until it fits there. Card scripts use it too.
void ScreenCards_Label(const char* utf8, int y, uint16_t color, uint8_t size);
