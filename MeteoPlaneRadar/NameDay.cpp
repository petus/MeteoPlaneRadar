// =============================================================================
//  MeteoPlaneRadar
//  Name days (0.9.2). See NameDay.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "NameDay.h"
#include "NameDays.h"
#include "Lang.h"
#include <string.h>

static const uint8_t DIM[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

bool NameDay_Get(uint8_t lang, int month, int day, bool ascii,
                 char* out, size_t cap, bool* holiday) {
  if (cap) out[0] = '\0';
  if (holiday) *holiday = false;
  if (lang != LANG_CZ && lang != LANG_SK) return false;
  if (month < 1 || month > 12 || day < 1 || day > DIM[month - 1]) return false;
  int idx = day - 1;
  for (int m = 0; m < month - 1; m++) idx += DIM[m];
  const char* s = (lang == LANG_SK) ? NAMEDAYS_SK[idx] : NAMEDAYS_CS[idx];
  if (!s || !*s) return false;
  if (*s == '*') { if (holiday) *holiday = true; s++; }
  if (ascii) Lang_ToAscii(out, cap, s);
  else { strncpy(out, s, cap - 1); out[cap - 1] = '\0'; }
  return out[0] != '\0';
}

bool NameDay_DropLast(char* list) {
  char* c = strrchr(list, ',');
  if (!c) return false;
  *c = '\0';
  return true;
}
