// =============================================================================
//  MeteoPlaneRadar
//  Spot price + generation mix. See Energy.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Energy.h"
#include "Net.h"
#include "Settings.h"
#include "Outside.h"
#include "Lang.h"
#include "Status.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

// --- Price state ------------------------------------------------------------
static float s_today[PRICE_SLOTS];
static float s_tomorrow[PRICE_SLOTS];
static bool  s_todayOk = false;
static bool  s_tomorrowOk = false;

// Which local day the arrays belong to (tm_yday). The answer says "today" and
// "tomorrow" without dating them, so the only way to notice that midnight has
// passed is to remember which day we asked on.
static int s_priceYday = -1;

static unsigned long s_pLastTry = 0;
static bool          s_pEverTried = false;

// --- Mix state --------------------------------------------------------------
static float  s_mix[MIX_GROUPS];
static bool   s_mixOk = false;
static float  s_renew = -1;
static float  s_load = NAN;
static float  s_export = NAN;
static time_t s_mixAt = 0;

static unsigned long s_mLastTry = 0;
static bool          s_mEverTried = false;

static bool s_force = false;

void Energy_Invalidate() { s_force = true; }

// -----------------------------------------------------------------------------
//  Where the two sources apply
// -----------------------------------------------------------------------------
static bool inBox(float latMin, float latMax, float lonMin, float lonMax) {
  const double la = Settings_Lat(), lo = Settings_Lon();
  return la >= latMin && la <= latMax && lo >= lonMin && lo <= lonMax;
}

bool Energy_PriceInArea() {
  return inBox(PRICE_AREA_LAT_MIN, PRICE_AREA_LAT_MAX,
               PRICE_AREA_LON_MIN, PRICE_AREA_LON_MAX);
}

bool Energy_MixInArea() {
  return inBox(MIX_AREA_LAT_MIN, MIX_AREA_LAT_MAX,
               MIX_AREA_LON_MIN, MIX_AREA_LON_MAX);
}

// -----------------------------------------------------------------------------
//  Price - readers
// -----------------------------------------------------------------------------
bool Energy_PriceValid()    { return s_todayOk; }
bool Energy_TomorrowValid() { return s_tomorrowOk; }

float Energy_PriceToday(int slot) {
  if (!s_todayOk || slot < 0 || slot >= PRICE_SLOTS) return NAN;
  return s_today[slot];
}
float Energy_PriceTomorrow(int slot) {
  if (!s_tomorrowOk || slot < 0 || slot >= PRICE_SLOTS) return NAN;
  return s_tomorrow[slot];
}

float Energy_PriceNow() {
  if (!s_todayOk || !Outside_TimeValid()) return NAN;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  return Energy_PriceToday(Price_SlotOf(lt.tm_hour, lt.tm_min));
}

static const float* dayArray(int day, bool* ok) {
  if (day == 0) { *ok = s_todayOk;    return s_today; }
  if (day == 1) { *ok = s_tomorrowOk; return s_tomorrow; }
  *ok = false;
  return s_today;
}

int Energy_CheapestSlot(int day) {
  bool ok; const float* a = dayArray(day, &ok);
  if (!ok) return -1;
  int best = -1;
  for (int i = 0; i < PRICE_SLOTS; i++) {
    if (isnan(a[i])) continue;
    if (best < 0 || a[i] < a[best]) best = i;
  }
  return best;
}

int Energy_PriceHighestSlot(int day) {
  bool ok; const float* a = dayArray(day, &ok);
  if (!ok) return -1;
  int best = -1;
  for (int i = 0; i < PRICE_SLOTS; i++) {
    if (isnan(a[i])) continue;
    if (best < 0 || a[i] > a[best]) best = i;
  }
  return best;
}

bool Energy_DayRange(int day, float* lo, float* hi) {
  bool ok; const float* a = dayArray(day, &ok);
  if (!ok) return false;
  float mn = NAN, mx = NAN;
  for (int i = 0; i < PRICE_SLOTS; i++) {
    if (isnan(a[i])) continue;
    if (isnan(mn) || a[i] < mn) mn = a[i];
    if (isnan(mx) || a[i] > mx) mx = a[i];
  }
  if (isnan(mn)) return false;
  if (lo) *lo = mn;
  if (hi) *hi = mx;
  return true;
}

float Energy_FinalCzkKwh(float spotCzkMwh) {
  if (isnan(spotCzkMwh)) return NAN;
  // The fee is stored in CZK/MWh so it can be added before the conversion and
  // without a second rounding step.
  float v = spotCzkMwh + (float)Settings_PriceFee();
  const uint8_t vat = Settings_PriceVat();
  if (vat) v *= (1.0f + vat / 100.0f);
  return v / 1000.0f;
}

// -----------------------------------------------------------------------------
//  Price - fetch
// -----------------------------------------------------------------------------
static void clearDay(float* a) {
  for (int i = 0; i < PRICE_SLOTS; i++) a[i] = NAN;
}

// Read one of the two day arrays. Returns how many slots were actually filled,
// which is how "tomorrow is not published yet" is told apart from a parse
// failure: the key exists and is simply an empty array.
//
// "minute" is treated as optional on purpose. The quarter-hour endpoint always
// sends it; the older hourly one never did. Without it every entry lands on the
// first slot of its hour, which is wrong but not nonsense - and far better than
// a screen that goes blank because a field moved.
static int readSlots(JsonArrayConst src, float* dst) {
  clearDay(dst);
  if (src.isNull()) return 0;
  int n = 0;
  for (JsonObjectConst e : src) {
    JsonVariantConst hv = e["hour"];
    JsonVariantConst pv = e["priceCZK"];
    if (hv.isNull() || pv.isNull()) continue;
    const int h = hv.as<int>();
    const int m = e["minute"] | 0;
    if (h < 0 || h > 23 || m < 0 || m > 59) continue;
    const int slot = Price_SlotOf(h, m);
    if (slot < 0 || slot >= PRICE_SLOTS) continue;
    dst[slot] = pv.as<float>();
    n++;
  }
  return n;
}

static bool fetchPrice() {
  String body;
  if (!Net_GetString(PRICE_URL, body, "CENA")) {
    Status_Set(ST_PRICE, "chyba stahovani");
    return false;
  }

  JsonDocument filter;
  JsonObject ft = filter["hoursToday"].add<JsonObject>();
  ft["hour"] = true; ft["minute"] = true; ft["priceCZK"] = true;
  JsonObject fm = filter["hoursTomorrow"].add<JsonObject>();
  fm["hour"] = true; fm["minute"] = true; fm["priceCZK"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  body = String();
  if (err) {
    Serial.printf("CENA: JSON %s\n", err.c_str());
    Status_Set(ST_PRICE, "chybny JSON");
    return false;
  }

  const int nT = readSlots(doc["hoursToday"].as<JsonArrayConst>(), s_today);
  const int nZ = readSlots(doc["hoursTomorrow"].as<JsonArrayConst>(), s_tomorrow);

  s_todayOk    = (nT > 0);
  s_tomorrowOk = (nZ > 0);

  // Stamp the arrays with the day they describe, so the rollover check in
  // Energy_Tick() has something to compare against.
  if (s_todayOk && Outside_TimeValid()) {
    time_t now = time(nullptr);
    struct tm lt; localtime_r(&now, &lt);
    s_priceYday = lt.tm_yday;
  }

  Serial.printf("Cena: dnes %d ctvrthodin, zitra %d\n", nT, nZ);
  if (s_todayOk) Status_Set(ST_PRICE, "OK, dnes %d / zitra %d ctvrthodin", nT, nZ);
  else           Status_Set(ST_PRICE, "prazdna odpoved");
  return s_todayOk;
}

// Midnight. Tomorrow's prices - if we have them - become today's, immediately,
// rather than after the next poll: at 00:05 the screen must not still be
// showing yesterday.
static void rollOverIfNewDay() {
  if (!Outside_TimeValid()) return;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  if (s_priceYday < 0 || lt.tm_yday == s_priceYday) return;

  if (s_tomorrowOk) {
    for (int i = 0; i < PRICE_SLOTS; i++) s_today[i] = s_tomorrow[i];
    s_todayOk = true;
  } else {
    s_todayOk = false;
    clearDay(s_today);
  }
  clearDay(s_tomorrow);
  s_tomorrowOk = false;
  s_priceYday = lt.tm_yday;
  s_pEverTried = false;              // and confirm it from the source at once
  Serial.println("Cena: novy den, posouvam zitrek na dnesek");
}

// -----------------------------------------------------------------------------
//  Mix - readers
// -----------------------------------------------------------------------------
bool  Energy_MixValid() { return s_mixOk; }
float Energy_MixMw(uint8_t g) { return (g < MIX_GROUPS) ? s_mix[g] : 0.0f; }

float Energy_MixTotalMw() {
  float t = 0;
  for (int i = 0; i < MIX_GROUPS; i++) if (s_mix[i] > 0) t += s_mix[i];
  return t;
}

float Energy_MixShare(uint8_t g) {
  const float t = Energy_MixTotalMw();
  if (t <= 0 || g >= MIX_GROUPS || s_mix[g] <= 0) return 0.0f;
  return s_mix[g] * 100.0f / t;
}

float  Energy_RenewableShare() { return s_renew; }
float  Energy_LoadMw()         { return s_load; }
float  Energy_ExportMw()       { return s_export; }
time_t Energy_MixSampleTime()  { return s_mixAt; }

const char* Energy_GroupName(uint8_t g) {
  switch (g) {
    case MIX_NUCLEAR: return T(S_NUCLEAR);
    case MIX_COAL:    return T(S_COAL);
    case MIX_GAS:     return T(S_GAS);
    case MIX_SOLAR:   return T(S_SOLAR);
    case MIX_WIND:    return T(S_WIND);
    case MIX_HYDRO:   return T(S_HYDRO);
    case MIX_BIOMASS: return T(S_BIOMASS);
    default:          return T(S_OTHER);
  }
}

// -----------------------------------------------------------------------------
//  Mix - fetch
// -----------------------------------------------------------------------------
// --- Production types -> our eight groups ------------------------------------
//
// 0.7.6: this used to compare the API's name with strcmp() against the spelling
// written out by hand, and it got one of them wrong - "Fossil brown coal /
// lignite" really does have spaces around the slash. Brown coal is a QUARTER of
// Czech generation, and because an unmatched type was silently dropped, the
// screen simply divided everything by a smaller total: solar read 40% when it
// was 30%, coal read under one per cent when it was 25. Every number on the
// dial was wrong and nothing anywhere said so.
//
// Two changes so that cannot happen again.
//
// FIRST, names are compared NORMALISED - lower case, and everything that is not
// a letter or a digit thrown away. "Fossil brown coal / lignite", "Fossil brown
// coal/lignite" and "FOSSIL BROWN COAL-LIGNITE" all reduce to the same key, so
// punctuation and spacing on the far end stop being load-bearing. The keys
// below are written already normalised, which is why they look the way they do.
//
// SECOND, and more important: an unknown generation type is now counted into
// MIX_OTHER and its name printed to the serial log, instead of vanishing. A new
// or renamed type then shows up as a bigger grey slice - visible, and wrong in
// the direction that makes someone ask - rather than as numbers that are all
// quietly a third too big.
struct TypeMap { const char* key; uint8_t group; };
static const TypeMap MIXMAP[] = {
  { "nuclear",                 MIX_NUCLEAR },
  { "fossilbrowncoallignite",  MIX_COAL    },
  { "fossilhardcoal",          MIX_COAL    },
  { "fossilcoalderivedgas",    MIX_COAL    },
  { "fossilpeat",              MIX_COAL    },
  { "fossilgas",               MIX_GAS     },
  { "fossiloil",               MIX_GAS     },
  { "fossiloilshale",          MIX_GAS     },
  { "solar",                   MIX_SOLAR   },
  { "windonshore",             MIX_WIND    },
  { "windoffshore",            MIX_WIND    },
  { "hydrorunofriver",         MIX_HYDRO   },
  { "hydrowaterreservoir",     MIX_HYDRO   },
  { "hydropumpedstorage",      MIX_HYDRO   },
  { "marine",                  MIX_HYDRO   },
  { "biomass",                 MIX_BIOMASS },
  { "waste",                   MIX_OTHER   },
  { "geothermal",              MIX_OTHER   },
  { "otherrenewables",         MIX_OTHER   },
  { "others",                  MIX_OTHER   },
};

// Series that are NOT generation and must never reach the ring. The two shares
// and the load are statistics about the mix; pumped-storage consumption is
// negative by definition and adding it to a ring of positive slices would make
// the percentages lie. Handled before the map, so the unknown-type fallback
// below cannot sweep them into "other".
static const char* const MIXSKIP[] = {
  "load",
  "residualload",
  "renewableshareofload",
  "hydropumpedstorageconsumption",
};

// Lower case, letters and digits only. Long names are truncated rather than
// overflowing - the keys above are all well inside the buffer.
static void normaliseType(const char* in, char* out, size_t cap) {
  size_t n = 0;
  for (const char* p = in; *p && n + 1 < cap; p++) {
    const unsigned char c = (unsigned char)*p;
    if (c >= 'A' && c <= 'Z')      out[n++] = (char)(c - 'A' + 'a');
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) out[n++] = (char)c;
  }
  out[n] = '\0';
}

// The newest value in one type's array. The tail of the window is usually a
// run of nulls - the most recent quarter hour has not been published for every
// type at the same moment - so "the last element" is the wrong answer and
// "the last element that is not null" is the right one.
static bool lastValue(JsonArrayConst data, float* out, int* atIndex) {
  if (data.isNull()) return false;
  const int n = (int)data.size();
  for (int i = n - 1; i >= 0; i--) {
    JsonVariantConst v = data[i];
    if (v.isNull()) continue;
    *out = v.as<float>();
    if (atIndex) *atIndex = i;
    return true;
  }
  return false;
}

static bool fetchMix() {
  // Without a clock there is no window to ask for. The clock is seeded by the
  // "Date" header of any other request, so this resolves itself within a poll
  // or two of coming online.
  if (!Outside_TimeValid()) return false;

  const time_t now = time(nullptr);
  const time_t to  = now + 3600;           // a little ahead, harmless if empty

  // Narrow window first, wider ones only if it comes back empty (energy-charts
  // answers an empty window with 404 "no content available") - see Config.h.
  static const int WINDOWS[] = MIX_WINDOWS_H;
  String body;
  bool got = false;
  for (int w : WINDOWS) {
    const time_t from = now - (time_t)w * 3600;
    struct tm a, b;
    gmtime_r(&from, &a);
    gmtime_r(&to,   &b);
    char url[256];
    snprintf(url, sizeof(url),
      "%s?country=%s&start=%04d-%02d-%02dT%02d:%02dZ&end=%04d-%02d-%02dT%02d:%02dZ",
      MIX_URL, Settings_MixCountry(),
      a.tm_year + 1900, a.tm_mon + 1, a.tm_mday, a.tm_hour, a.tm_min,
      b.tm_year + 1900, b.tm_mon + 1, b.tm_mday, b.tm_hour, b.tm_min);
    if (Net_GetString(url, body, "MIX")) { got = true; break; }
    Serial.printf("MIX: za poslednich %d h nic, zkousim delsi okno\n", w);
  }
  if (!got) {
    Status_Set(ST_MIX, "zadna data (zdroj se zpozduje)");
    return false;
  }

  JsonDocument filter;
  filter["unix_seconds"] = true;
  JsonObject fp = filter["production_types"].add<JsonObject>();
  fp["name"] = true;
  fp["data"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body,
                                             DeserializationOption::Filter(filter));
  body = String();
  if (err) {
    Serial.printf("MIX: JSON %s\n", err.c_str());
    Status_Set(ST_MIX, "chybny JSON");
    return false;
  }

  float next[MIX_GROUPS] = {0};
  float renew = -1, load = NAN, expo = NAN;
  int   newest = -1;                 // index of the freshest sample we used
  int   used = 0;

  JsonArrayConst types = doc["production_types"];
  if (types.isNull()) { Status_Set(ST_MIX, "prazdna odpoved"); return false; }

  for (JsonObjectConst t : types) {
    const char* name = t["name"] | "";
    if (!*name) continue;
    JsonArrayConst data = t["data"];
    float v; int at = -1;
    if (!lastValue(data, &v, &at)) continue;
    if (at > newest) newest = at;

    char key[48];
    normaliseType(name, key, sizeof(key));

    if (!strcmp(key, "crossborderelectricitytrading")) {
      // The API counts imports as positive; the screen talks about export, so
      // the sign is flipped once, here, rather than in the drawing code.
      expo = -v;
      continue;
    }
    if (!strcmp(key, "renewableshareofgeneration")) { renew = v; continue; }
    if (!strcmp(key, "load"))                       { load = v; continue; }

    bool skip = false;
    for (auto& s : MIXSKIP) if (!strcmp(key, s)) { skip = true; break; }
    if (skip) continue;

    int group = -1;
    for (auto& m : MIXMAP) if (!strcmp(key, m.key)) { group = m.group; break; }

    if (group < 0) {
      // Unknown generation type. Count it rather than drop it, and say so -
      // this is the line that would have caught the brown-coal bug on the first
      // boot instead of on a photograph of the screen weeks later.
      group = MIX_OTHER;
      Serial.printf("MIX: neznamy typ \"%s\" (%.0f MW) -> ostatni\n", name, v);
    }

    if (v > 0) next[group] += v;        // a negative generation figure is noise
    used++;
  }

  float total = 0;
  for (int i = 0; i < MIX_GROUPS; i++) total += next[i];
  if (used == 0 || total <= 0) {
    // Everything parsed but there was no generation in the window - almost
    // always means the window fell into a gap. Keep whatever we had on screen.
    Status_Set(ST_MIX, "zadna data v okne");
    return false;
  }

  for (int i = 0; i < MIX_GROUPS; i++) s_mix[i] = next[i];
  s_renew = renew;
  s_load  = load;
  s_export = expo;
  s_mixOk = true;

  JsonArrayConst secs = doc["unix_seconds"];
  s_mixAt = 0;
  if (!secs.isNull() && newest >= 0 && newest < (int)secs.size())
    s_mixAt = (time_t)secs[newest].as<long long>();

  // --- Does the mix add up? ---------------------------------------------------
  // Everything generated either gets used here or leaves the country, so the sum
  // of the slices should land near load plus exports. It is not an identity -
  // losses, pumping and the odd unreported megawatt all sit in the gap - but it
  // is close enough that a missing PRODUCTION TYPE stands out immediately: with
  // brown coal dropped the two sides were 1.6 GW apart, an eighteen per cent
  // hole that nothing on the screen mentioned.
  //
  // This does not reject the data. A ring drawn from a slightly-off total is
  // still worth more than a blank screen, and the source is the one place where
  // a gap might be real. It puts the discrepancy on the status page, where
  // someone looking for an explanation will find one.
  bool suspect = false;
  if (!isnan(load) && !isnan(expo)) {
    const float expect = load + expo;
    if (expect > 500.0f) {
      const float offBy = fabsf(total - expect) / expect;
      if (offBy > MIX_SANITY_TOL) {
        suspect = true;
        Serial.printf("MIX: VAROVANI, vyroba %.0f MW proti spotreba+vyvoz %.0f MW "
                      "(rozdil %.0f %%) - chybi typ zdroje?\n",
                      total, expect, offBy * 100.0f);
      }
    }
  }

  Serial.printf("Mix: %.0f MW celkem, OZE %.0f %%, spotreba %.0f MW\n",
                total, renew, load);
  if (suspect) Status_Set(ST_MIX, "%.1f GW / OZE %.0f %% - soucet nesedi",
                          total / 1000.0f, renew);
  else         Status_Set(ST_MIX, "OK, %.1f GW / OZE %.0f %%",
                          total / 1000.0f, renew);
  return true;
}

// -----------------------------------------------------------------------------
void Energy_Tick() {
  // Same rule as the forecast: with no link, do not even start the timers, or
  // an attempt made while WiFi was still coming up would count as "tried" and
  // push the real one half an hour out.
  if (WiFi.status() != WL_CONNECTED) return;

  // Nothing here is worth a single byte of traffic while both screens are off.
  // The settings are what the user is telling us they care about.
  // Two questions, both of which have to be yes: has the user asked for this
  // screen, and can the source answer for where the device is? The second one
  // matters as much as the first. Quietly polling a Czech volunteer API every
  // half hour from a device that is in another continent, for numbers nobody
  // there can use, is not something to do by accident.
  const bool wantPrice = Settings_ScreenEnabled(SCREEN_PRICE_I) && Energy_PriceInArea();
  const bool wantMix   = Settings_ScreenEnabled(SCREEN_MIX_I)   && Energy_MixInArea();
  if (!wantPrice && !wantMix) return;

  if (s_force) { s_pEverTried = false; s_mEverTried = false; s_force = false; }

  rollOverIfNewDay();

  unsigned long now = millis();

  if (wantPrice &&
      (!s_pEverTried || now - s_pLastTry >= (s_todayOk ? PRICE_PERIOD_MS : PRICE_RETRY_MS))) {
    s_pEverTried = true;
    s_pLastTry = now;
    fetchPrice();
    return;                          // one request per tick, never two in a row
  }

  if (wantMix &&
      (!s_mEverTried || now - s_mLastTry >= (s_mixOk ? MIX_PERIOD_MS : MIX_RETRY_MS))) {
    s_mEverTried = true;
    s_mLastTry = now;
    fetchMix();
  }
}
