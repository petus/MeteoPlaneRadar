// =============================================================================
//  MeteoPlaneRadar
//  JSON cards - small user-installed screens described by a JSON file.
//
//  A card is DATA, not code: a title, one URL that returns JSON, and up to four
//  rows saying which value of the answer to show and how. The firmware fetches
//  the URL, picks the values out (CardPath.h) and lays them out itself. That is
//  what makes a card safe to install from someone else - the worst it can do is
//  show a wrong number - and small: a card is a few hundred bytes.
//
//  Cards live as /cards/<id>.json on a LittleFS in the "spiffs" partition,
//  which partitions.csv has always had and nothing used. They are installed
//  and removed from the web UI (WebConfig.cpp) and shown by ScreenCards.
//
//  The whole feature is behind the "Cards" screen switch, which is off by
//  default: a device whose owner never asked for it makes no extra requests.
//
//  The format is described in cards/README.md next to the sample cards.
// =============================================================================
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "Config.h"

struct CardRow {
  char    label[20];
  char    path[72];
  char    unit[10];
  int8_t  decimals;        // -1 = as received
  float   scale;           // multiplied in before formatting (1 = leave alone)
  char    value[24];       // formatted, "" until the first good fetch
  bool    numeric;         // value is a formatted number (decimal comma in CZ)
};

struct Card {
  char     id[24];
  char     title[28];
  char     url[200];
  uint16_t refreshMin;
  uint8_t  nRows;
  CardRow  rows[CARD_ROWS_MAX];
  char     noteLabel[20];
  char     notePath[72];
  char     note[28];

  bool          valid;         // values are from a successful fetch
  bool          stale;         // ...but the most recent attempt failed
  bool          everTried;
  unsigned long lastTry;
  unsigned long lastOk;        // millis() of the last successful fetch

  // Script cards (Script.h). The program itself stays in the card's file.
  bool          hasScript;
  uint16_t      redrawS;       // redraw every so many seconds (0 = only when the data changes)
  uint8_t       nKeep;
  char          keep[CARD_KEEP_MAX][72];   // what of the answer the script gets (all of it if none)
  char*         data;          // that part of the last answer, as JSON text
  uint32_t      dataRev;       // goes up with every new answer; 0 = none yet
  uint8_t       scriptFails;   // failures in a row
  uint32_t      installNo;     // differs for every install, so a replaced card is told from the old one
};

void Cards_Begin();            // mount the filesystem and load the cards
void Cards_Tick();             // at most one request per call
bool Cards_FsReady();

int         Cards_Count();
const Card* Cards_Get(int i);  // nullptr when out of range
// Changes whenever the list or any value does - the screen redraws on it.
uint32_t    Cards_Revision();

// Script cards: the JSON the card kept from its last answer (nullptr before
// the first one), and the card itself for the script runner's bookkeeping.
const char* Cards_Data(const Card* c);
Card*       Cards_Mutable(const char* id);

// Validates and stores a card (replacing one with the same id). On failure
// err holds a short English reason for the web UI.
bool Cards_Install(JsonObjectConst card, char* err, size_t errCap);
bool Cards_Remove(const char* id);
