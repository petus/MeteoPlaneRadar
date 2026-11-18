// =============================================================================
//  MeteoPlaneRadar
//  Screen: Czech generation mix. See ScreenMix.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#include "ScreenMix.h"
#include "Energy.h"
#include "Settings.h"
#include "Outside.h"
#include "Layout.h"
#include "Lang.h"
#include "SrcHealth.h"
#include "UI.h"
#include "AAFont.h"     // AAText_Enabled: accents or not
#include "Display_ST7701.h"
#include "Config.h"

#include <WiFi.h>
#include <time.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

// The ring is deliberately SMALLER than it wants to be, and everything else is
// inside it.
//
// 0.7.0 drew each label as one line of size-1 text just outside a fat ring, and
// on the panel the two ran into each other: eight pixels of grey sitting on the
// edge of a bright sector is unreadable whatever it says. The fix is not a
// bigger gap - it is a thinner ring and bigger labels. Each label is now two
// lines at size 2, the source above its percentage, with the ring pulled in far
// enough that the block has clear black around it.
//
// The trade figure moved into the middle for the same reason: at the foot of
// the panel it was competing with the bottom label for the one spot where the
// circle is narrowest.
#define MCX 240
#define MCY 246
#define R_OUT 142
#define R_IN  104

// The middle, top to bottom. Note what is NOT up at LY_SUB where the other
// screens put a caption: the sample time. It used to be a line of chrome across
// the top of the glass, which is exactly where the small slices of a Czech mix
// want to put their labels - waste, hydro and wind are neighbours in the enum,
// so they arrive as a knot just before twelve o'clock and then find a caption
// sitting in their way. Moving it inside the ring frees the whole top band for
// labels and puts the timestamp next to the numbers it qualifies, at a size
// that can actually be read.
#define COUNTRY_Y 172     // 0.9.4: the country, above the renewable share
#define OZE_LBL_Y 192
#define OZE_BIG_Y 204
#define LOAD_Y    244
#define TRADE_Y   268
#define TIME_Y    288

// Label placement. See drawGroupLabel() for what these mean - they are not
// coordinates, they are the two limits the radius is solved against.
#define LBL_GAP   6      // black that must stay between the ring and a label
#define LBL_R_MAX 234    // furthest the block's outer corner may reach
#define LBL_PAD    4     // padding inside a label's claimed box

// Every label is drawn at the size its own share earns. A sliver worth half a
// per cent that happened to land in an empty quarter of the screen must not
// shout louder than a tenth of the country's generation next to it - label
// size is information, not decoration.
#define LBL_HUGE_PCT 20.0f    // the one or two sources actually running the grid
#define LBL_BIG_PCT   5.0f

// A slice smaller than this gets no label at all. Half a per cent is about a
// two-pixel sliver on this ring - there is nothing there to point at.
//
// Everything above it does get named, however small, because "what is that blue
// wedge?" is a question the screen should answer rather than leave hanging. A
// share under one per cent is written "<1%": rounding 0.7 to "1%" would be a
// worse answer than admitting the number is small.
#define LABEL_MIN_PCT 0.5f

// 0.9.4: the country by name, in the language of the screen - the same list as
// the web page offers. Shown at the top of the disc, so a Slovak or a German
// grid is never mistaken for the Czech one.
static const struct { const char cc[3]; const char *cs, *en, *sk; } COUNTRIES[] = {
  {"eu", "Evropa",         "Europe",         "Európa"},
  {"cz", "Česko",          "Czechia",        "Česko"},
  {"sk", "Slovensko",      "Slovakia",       "Slovensko"},
  {"at", "Rakousko",       "Austria",        "Rakúsko"},
  {"de", "Německo",        "Germany",        "Nemecko"},
  {"pl", "Polsko",         "Poland",         "Poľsko"},
  {"hu", "Maďarsko",       "Hungary",        "Maďarsko"},
  {"si", "Slovinsko",      "Slovenia",       "Slovinsko"},
  {"hr", "Chorvatsko",     "Croatia",        "Chorvátsko"},
  {"be", "Belgie",         "Belgium",        "Belgicko"},
  {"bg", "Bulharsko",      "Bulgaria",       "Bulharsko"},
  {"ch", "Švýcarsko",      "Switzerland",    "Švajčiarsko"},
  {"dk", "Dánsko",         "Denmark",        "Dánsko"},
  {"ee", "Estonsko",       "Estonia",        "Estónsko"},
  {"es", "Španělsko",      "Spain",          "Španielsko"},
  {"fi", "Finsko",         "Finland",        "Fínsko"},
  {"fr", "Francie",        "France",         "Francúzsko"},
  {"gr", "Řecko",          "Greece",         "Grécko"},
  {"ie", "Irsko",          "Ireland",        "Írsko"},
  {"it", "Itálie",         "Italy",          "Taliansko"},
  {"lt", "Litva",          "Lithuania",      "Litva"},
  {"lu", "Lucembursko",    "Luxembourg",     "Luxembursko"},
  {"lv", "Lotyšsko",       "Latvia",         "Lotyšsko"},
  {"nl", "Nizozemsko",     "Netherlands",    "Holandsko"},
  {"no", "Norsko",         "Norway",         "Nórsko"},
  {"pt", "Portugalsko",    "Portugal",       "Portugalsko"},
  {"ro", "Rumunsko",       "Romania",        "Rumunsko"},
  {"rs", "Srbsko",         "Serbia",         "Srbsko"},
  {"se", "Švédsko",        "Sweden",         "Švédsko"},
  {"ua", "Ukrajina",       "Ukraine",        "Ukrajina"},
  {"uk", "Velká Británie", "United Kingdom", "Veľká Británia"},
};

// The country's name for the active language and font - without accents for
// the built-in pixel font. An unknown code comes back upper-cased ("XY").
static const char* countryName(char* buf, size_t cap) {
  const char* cc = Settings_MixCountry();
  const char* name = nullptr;
  for (const auto& c : COUNTRIES) {
    if (!strcmp(c.cc, cc)) {
      name = Lang_Get() == LANG_EN ? c.en : (Lang_Get() == LANG_SK ? c.sk : c.cs);
      break;
    }
  }
  if (!name) {
    snprintf(buf, cap, "%c%c", (char)toupper((unsigned char)cc[0]),
             (char)toupper((unsigned char)cc[1]));
    return buf;
  }
  if (AAText_Enabled()) { snprintf(buf, cap, "%s", name); return buf; }
  Lang_ToAscii(buf, cap, name);
  return buf;
}

// The heading while there is nothing to draw yet (loading, outside Europe).
static const char* mixTitle(char* buf, size_t cap) {
  const char* cc = Settings_MixCountry();
  if (!strcmp(cc, "cz")) return T(S_MIX);
  char nm[40];
  countryName(nm, sizeof(nm));
  snprintf(buf, cap, "%s - %s", T(S_MIX_GEN), nm);
  return buf;
}

static bool s_lastValid = false;
static time_t s_lastAt = 0;

void ScreenMix_Enter() { s_lastAt = 0; }

bool ScreenMix_Tick() {
  bool want = false;

  const bool valid = Energy_MixValid();
  const time_t at = Energy_MixSampleTime();
  if (valid != s_lastValid || at != s_lastAt) {
    s_lastValid = valid;
    s_lastAt = at;
    want = true;
  }

  // The mix itself only moves every quarter of an hour, so without this the
  // clock at the top of the screen would sit still for fifteen minutes at a
  // time and look stopped.
  if (UI_StatusLineChanged()) want = true;

  return want;
}

// One colour per group, in the order of Energy.h's MixGroup.
//
// Chosen to be told apart at a glance rather than to be pretty: the fossil
// sources are warm, the renewables are cool, nuclear is neither. That way the
// ring still says something useful to someone who cannot read the labels from
// across the room.
static const uint16_t GROUP_COLOR[MIX_GROUPS] = {
  0x92DB,   // nuclear  - violet
  0x836B,   // coal     - brown
  0xF465,   // gas      - orange
  0xFE85,   // solar    - yellow
  0x5DFE,   // wind     - pale blue
  0x2B7B,   // hydro    - blue
  0x5DAA,   // biomass  - green
  C_GRAY    // other
};


// One group label: the source on top, its share underneath.
//
// The sizes are NOT fixed, and that is the whole point of this function.
//
// 0.7.1 put every label at size 2 on a fixed radius and they ran into the ring
// anyway. The reason is geometry that is easy to get wrong: a label is an
// UPRIGHT RECTANGLE placed on a radius, so its nearest CORNER sits closer to
// the centre than its centre does - by (w/2)|cos| + (h/2)|sin|. At the sides of
// the dial, where |cos| is 1, a 92 px wide block reaches 46 px further in than
// its own centre. Checking that the block fitted inside the glass, as 0.7.1
// did, says nothing about whether it clears the ring.
//
// So the radius is solved rather than fixed: pushed out until the near corner
// clears the ring by LBL_GAP, and rejected if the far corner then leaves the
// glass. A label at three o'clock therefore sits further out than one at
// twelve, which is exactly right - it needs the room.
//
// When even that fails, the label gets SMALLER rather than disappearing, which
// is the other half of the fix. The tiers are ordered so the first thing to go
// is the size of the NAME, not of the number: the number is what is being read,
// the name is a word you recognise by its shape and its colour. In practice
// tier 2 clears the ring at every angle for every name, so the label is only
// ever dropped when a neighbour has genuinely taken the space.
// The NUMBER never drops below size 2. That was the mistake in 0.7.3: hydro on
// an ordinary day is under one per cent, so it started on the smallest tier and
// got a label 6 px tall - drawn, claimed, correct, and invisible from a metre
// away, which is the only distance this screen is ever read from. "Quieter
// than its neighbours" has to stop well short of "illegible"; what shrinks for
// a small share is the NAME, and that is the whole of it.
struct LblTier { uint8_t nameSize, valSize; };
static const LblTier LBL_TIERS[] = { {2, 3}, {2, 2}, {1, 2} };

// Where a label may go when its own spot is taken: a few degrees along the rim,
// or further out from the middle, or both. Ordered by how far that is from the
// slice it belongs to, so a label only moves as much as the crowding forces.
//
// The outward step is what makes the top of this dial work at all. The small
// sources are neighbours in the enum - waste, hydro, wind - so on the ring they
// arrive as a knot of slivers in one narrow arc, all wanting the same few
// square centimetres, while half the glass is empty black. Sliding sideways
// only shuffles the knot; letting the second row sit further out unpacks it.
struct LblSpot { int8_t nudgeDeg; uint8_t extraR; };
static const LblSpot LBL_SPOTS[] = {
  {  0,  0}, {  4,  0}, { -4,  0}, {  0, 20}, {  8,  0}, { -8,  0},
  {  4, 20}, { -4, 20}, { 12,  0}, {-12,  0}, {  8, 20}, { -8, 20},
  {  0, 38}, { 12, 20}, {-12, 20}, {  4, 38}, { -4, 38}, { 17,  0},
  {-17,  0}, {  8, 38}, { -8, 38}, { 17, 20}, {-17, 20}, { 12, 38},
  {-12, 38}, { 22,  0}, {-22,  0}, { 22, 20}, {-22, 20},
};

static void drawGroupLabel(uint8_t g, float midAngle, float pct) {
  char name[16], val[8];
  snprintf(name, sizeof(name), "%s", Energy_GroupName(g));
  if (pct < 0.95f) snprintf(val, sizeof(val), "<1%%");
  else             snprintf(val, sizeof(val), "%.0f%%", pct);

  // Where in the ladder this share is allowed to start. Four steps now: the
  // top one puts the NUMBER at size 3, which is worth having where the ring has
  // the room - on a typical day two sources carry two thirds of the country and
  // those two numbers are most of what the screen is for.
  const int first = (pct >= LBL_HUGE_PCT) ? 0
                  : (pct >= LBL_BIG_PCT)  ? 1 : 2;
  const int tiers = (int)(sizeof(LBL_TIERS) / sizeof(LBL_TIERS[0]));

  for (int i = first; i < tiers; i++) {
    const LblTier& t = LBL_TIERS[i];
    const int nh = LY_CHAR_H(t.nameSize);
    const int vh = LY_CHAR_H(t.valSize);
    const int nw = Layout_TextW(name, t.nameSize);
    const int vw = Layout_TextW(val,  t.valSize);
    const int w  = ((nw > vw) ? nw : vw) + LBL_PAD;
    const int h  = nh + 2 + vh + LBL_PAD;

    // Try the slice's own spot first, then the ones further from it.
    for (int k = 0; k < (int)(sizeof(LBL_SPOTS) / sizeof(LBL_SPOTS[0])); k++) {
      const LblSpot& sp = LBL_SPOTS[k];
      const float ang = midAngle + (float)sp.nudgeDeg * 0.0174532925f;
      const float c = cosf(ang), s = sinf(ang);

      // How far the near corner reaches back towards the centre, and the radius
      // that puts it exactly LBL_GAP clear of the ring - plus this spot's own
      // step outward.
      const float reach = (w / 2.0f) * fabsf(c) + (h / 2.0f) * fabsf(s);
      const int   foot  = R_OUT + LBL_GAP + (int)sp.extraR;   // inner edge of the block
      const int   R     = foot + (int)reach;
      if ((float)R + reach > (float)LBL_R_MAX) continue;  // far corner off the glass

      const int x0 = MCX + (int)(R * c) - w / 2;
      const int y0 = MCY + (int)(R * s) - h / 2;

      // Claimed rather than drawn outright: two narrow neighbouring slices can
      // want the same spot. Failing here is what sends us to the next angle,
      // and eventually to a smaller tier.
      if (!Layout_Claim(x0, y0, w, h)) continue;

      // A moved label needs to say which slice it belongs to. The leader runs
      // just outside the ring, from the slice's true middle to the foot of the
      // label, in the slice's own colour - short, and it cannot cross the ring
      // or the text.
      if (k != 0) {
        const float mc = cosf(midAngle), ms = sinf(midAngle);
        gfx->drawLine(MCX + (int)((R_OUT + 2) * mc), MCY + (int)((R_OUT + 2) * ms),
                      MCX + (int)(foot * c), MCY + (int)(foot * s),
                      GROUP_COLOR[g]);
      }

      // The name takes the group's colour so the eye can pair it with its
      // sector; the number is white, because that is the part being read.
      UI_TextCenteredIn(name, x0, w, y0 + LBL_PAD / 2,          GROUP_COLOR[g], t.nameSize);
      UI_TextCenteredIn(val,  x0, w, y0 + LBL_PAD / 2 + nh + 2, C_WHITE,        t.valSize);
      return;
    }
  }
  // Nothing fitted at any angle or size. The slice keeps its colour in the
  // ring; a label lying on top of its neighbour would tell you less than that.
}

static void drawRing() {
  const float total = Energy_MixTotalMw();
  if (total <= 0) return;

  // Start at the top and run clockwise, like every other dial in this project.
  float a = -90.0f * 0.0174532925f;

  for (int g = 0; g < MIX_GROUPS; g++) {
    const float mw = Energy_MixMw(g);
    if (mw <= 0) continue;
    const float sweep = (mw / total) * 2.0f * 3.14159265f;

    // The hair of black that separates the slices is a THIRD of a small slice
    // rather than a fixed angle. A flat 0.6 deg is nothing to a slice worth a
    // third of the country and most of a slice worth three quarters of a per
    // cent - hydro on an ordinary day is 2.7 deg wide, and taking a fixed gap
    // out of it left a wedge you had to look for. Now the separator can never
    // eat more than a third of whatever it is separating.
    float gap = 0.6f * 0.0174532925f;
    if (gap > sweep / 3.0f) gap = sweep / 3.0f;

    const float a0 = a + gap * 0.5f;
    const float a1 = a + sweep - gap * 0.5f;
    if (a1 > a0) UI_FillRing(MCX, MCY, R_IN, R_OUT, a0, a1, GROUP_COLOR[g]);
    a += sweep;
  }

  // Labels in a second pass, so a slice drawn later cannot paint over the label
  // of the one before it.
  //
  // BIGGEST FIRST, not in ring order. Where two neighbouring slices are too
  // close for both labels, whichever is placed first keeps the spot - and the
  // one worth keeping is the larger. In ring order a three per cent sliver
  // could take the place of the four per cent slice next to it purely because
  // it came earlier in the enum.
  struct Cand { uint8_t g; float mid; float pct; };
  Cand cand[MIX_GROUPS];
  int n = 0;

  a = -90.0f * 0.0174532925f;
  for (int g = 0; g < MIX_GROUPS; g++) {
    const float mw = Energy_MixMw(g);
    if (mw <= 0) continue;
    const float sweep = (mw / total) * 2.0f * 3.14159265f;
    const float pct = Energy_MixShare(g);
    if (pct >= LABEL_MIN_PCT) {
      cand[n].g = (uint8_t)g;
      cand[n].mid = a + sweep / 2.0f;
      cand[n].pct = pct;
      n++;
    }
    a += sweep;
  }

  // Insertion sort - eight entries at most, and it keeps equal shares in ring
  // order, which is one less thing to flicker between refreshes.
  for (int i = 1; i < n; i++) {
    Cand key = cand[i];
    int j = i - 1;
    while (j >= 0 && cand[j].pct < key.pct) { cand[j + 1] = cand[j]; j--; }
    cand[j + 1] = key;
  }

  for (int i = 0; i < n; i++) drawGroupLabel(cand[i].g, cand[i].mid, cand[i].pct);
}

// Does a centred line fit inside the ring at this height? Everything in the
// middle of this screen is measured against the DISC, not the glass - the ring
// is what it would run into.
static bool fitsInDisc(const char* s, uint8_t size, int y) {
  const int dy = y + LY_CHAR_H(size) - MCY;
  const long d2 = (long)(R_IN - 6) * (R_IN - 6) - (long)dy * dy;
  if (d2 <= 0) return false;
  return Layout_TextW(s, size) <= 2 * (int)sqrtf((float)d2) - 8;
}

// Megawatts are the unit the source uses, gigawatts are the unit the country is
// discussed in. Anything at or above a gigawatt is shown as GW with one decimal.
static void fmtPower(float mw, char* out, size_t cap) {
  if (isnan(mw)) { snprintf(out, cap, "--"); return; }
  const float a = fabsf(mw);
  if (a >= 1000.0f) snprintf(out, cap, "%.1f GW", mw / 1000.0f);
  else              snprintf(out, cap, "%.0f MW", mw);
  if (Lang_Get() != LANG_EN) for (char* p = out; *p; p++) if (*p == '.') *p = ',';
}

static void drawCentre() {
  char buf[40];

  // --- the country (0.9.4) ---
  // At the top of the disc. Size 2, or size 1 when a long name ("Velká
  // Británie", "United Kingdom") does not fit the chord there.
  {
    countryName(buf, sizeof(buf));
    const uint8_t sz = fitsInDisc(buf, 2, COUNTRY_Y) ? 2 : 1;
    Layout_ReserveTextCentered(buf, sz, MCX, COUNTRY_Y);
    UI_TextCentered(buf, COUNTRY_Y, C_WHITE, sz);
  }

  // --- renewable share ---
  // Taken from the source's own figure rather than summed from our groups: it
  // counts the renewable part of waste incineration, which our eight-group
  // rounding cannot. Falling back to the sum would quietly report a different
  // number from every other publication of the same data.
  const float renew = Energy_RenewableShare();
  if (renew >= 0) {
    // "14% OZE" on one line (0.9.4): the label at size 2 right of the number,
    // on its baseline. It used to be a size-1 line of its own, and size 1 in
    // a smooth face is squeezed into the 6 px cell of the built-in font - the
    // three capitals ran into each other.
    snprintf(buf, sizeof(buf), "%.0f%%", renew);
    const char* lbl = T(S_RENEWABLE);
    const int wb = Layout_TextW(buf, 4), wl = Layout_TextW(lbl, 2), gap = 8;
    const int tot = wb + gap + wl;
    const int dy = OZE_BIG_Y - MCY;                 // the top of the line is its narrowest
    const long d2 = (long)(R_IN - 6) * (R_IN - 6) - (long)dy * dy;
    const int room = d2 > 0 ? 2 * (int)sqrtf((float)d2) - 8 : 0;
    if (tot <= room) {
      const int x0 = MCX - tot / 2;
      Layout_Reserve(x0 - 2, OZE_BIG_Y - 2, tot + 4, LY_CHAR_H(4) + 4);
      gfx->setTextColor(C_GREEN);
      gfx->setTextSize(4);
      gfx->setCursor(x0, OZE_BIG_Y);
      gfx->print(buf);
      gfx->setTextColor(C_GRAY);
      gfx->setTextSize(2);
      gfx->setCursor(x0 + wb + gap, OZE_BIG_Y + LY_CHAR_H(4) - LY_CHAR_H(2));
      gfx->print(lbl);
    } else {
      // Too wide (English "renewable"): the label a size-2 line of its own and
      // the number a size smaller under it, both between the country and the
      // load line.
      Layout_ReserveTextCentered(lbl, 2, MCX, OZE_LBL_Y + 4);
      UI_TextCentered(lbl, OZE_LBL_Y + 4, C_GRAY, 2);
      Layout_ReserveTextCentered(buf, 3, MCX, OZE_LBL_Y + 22);
      UI_TextCentered(buf, OZE_LBL_Y + 22, C_GREEN, 3);
    }
  } else {
    // No share figure - show the total generated instead, so the middle of the
    // ring is never empty while the ring itself is full.
    fmtPower(Energy_MixTotalMw(), buf, sizeof(buf));
    Layout_ReserveTextCentered(buf, 3, MCX, OZE_BIG_Y + 6);
    UI_TextCentered(buf, OZE_BIG_Y + 6, C_WHITE, 3);
  }

  // --- load ---
  // Label and value on ONE line at size 2, not a size-2 number over a size-1
  // caption. It reads the same, it is a line shorter, and nothing on this
  // screen is written at size 1 any more - at arm's length that size is
  // decoration, not information.
  const float load = Energy_LoadMw();
  if (!isnan(load)) {
    char pw[24];
    fmtPower(load, pw, sizeof(pw));
    snprintf(buf, sizeof(buf), "%s %s", T(S_CONSUMPTION), pw);
    if (fitsInDisc(buf, 2, LOAD_Y)) {
      Layout_ReserveTextCentered(buf, 2, MCX, LOAD_Y);
      UI_TextCentered(buf, LOAD_Y, C_WHITE, 2);
    }
  }
}

// When the mix was measured. See the note above the layout constants for why
// this is here in the middle rather than captioning the top of the screen.
static void drawSampleTime() {
  const time_t at = Energy_MixSampleTime();
  if (!at || !Outside_TimeValid()) return;

  struct tm lt; localtime_r(&at, &lt);
  char line[32];
  // ENTSO-E publishes an hour or two behind real time, so this clock does NOT
  // agree with the one at the top of the screen and is not supposed to.
  // Spelling it "data z 10:45" rather than printing a bare time is the whole
  // difference between a caption and an apparent bug.
  snprintf(line, sizeof(line), "%s %02d:%02d", T(S_DATA_FROM), lt.tm_hour, lt.tm_min);
  // In 0.7.5 this line silently lost by TWO PIXELS - 144 px of text into 142 px
  // of disc - and simply never appeared, which took a photograph of the screen
  // to notice. The row moved up to give it four pixels of margin, and a line
  // that still does not fit now says so on the serial console instead of
  // vanishing without trace.
  if (!fitsInDisc(line, 2, TIME_Y)) {
    Serial.printf("MIX: cas vzorku \"%s\" se nevejde do kruhu, vynechavam\n", line);
    return;
  }
  Layout_ReserveTextCentered(line, 2, MCX, TIME_Y);
  // 0.9.7: orange once the sample is over 3 hours old - ENTSO-E is normally
  // one or two behind, so this means the source has stopped, not that it lags.
  const bool old = time(nullptr) - at > 3 * 3600;
  UI_TextCentered(line, TIME_Y, old ? C_ORANGE : C_GRAY, 2);
}

// Export or import, the bottom line of the middle. Positive means the country
// is sending power out - see the sign note in Energy.cpp.
static void drawTrade() {
  const float ex = Energy_ExportMw();
  if (isnan(ex)) return;

  char val[24], line[40];
  fmtPower(fabsf(ex), val, sizeof(val));
  snprintf(line, sizeof(line), "%s %s", ex >= 0 ? T(S_EXPORT) : T(S_IMPORT), val);

  if (!fitsInDisc(line, 2, TRADE_Y)) return;
  Layout_ReserveTextCentered(line, 2, MCX, TRADE_Y);
  UI_TextCentered(line, TRADE_Y, ex >= 0 ? C_GREEN : C_ORANGE, 2);
}

void ScreenMix_Draw() {
  gfx->fillScreen(C_BLACK);
  Layout_Begin();

  Layout_ReserveBand(LY_DOTS - 6, 12);
  Layout_ReserveBand(LY_STATUS - 3, 22);
  UI_DrawStatusLine(LY_STATUS);

  char title[64];
  const char* head = mixTitle(title, sizeof(title));

  if (!Energy_MixInArea()) {
    UI_TextCentered(head, LCD_HEIGHT / 2 - 30, C_WHITE, 2);
    UI_TextCentered(T(S_MIX_EU_ONLY), LCD_HEIGHT / 2, C_YELLOW, 2);
    UI_TextCentered(T(S_OUT_OF_AREA), LCD_HEIGHT / 2 + 26, C_GRAY, 1);
    return;
  }

  if (!Energy_MixValid()) {
    UI_NoData(head, Energy_MixHealth());           // 0.9.7: not "Loading..." for ever
    return;
  }

  // Nothing is reserved at the top any more - the band belongs to the labels.
  drawRing();
  drawCentre();
  drawTrade();
  drawSampleTime();
}
