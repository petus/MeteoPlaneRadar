// =============================================================================
//  MeteoPlaneRadar
//  Exchange rates (0.9.6). See Fx.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Fx.h"
#include "FxParse.h"
#include "Net.h"
#include "Settings.h"
#include "Outside.h"
#include "Status.h"
#include "SrcHealth.h"
#include <WiFi.h>
#include <math.h>
#include <string.h>
#include <time.h>

static FxRow    s_rows[FX_MAX];
static int      s_n = 0;
static bool     s_ok = false;
static char     s_home[4] = "";
static FxSource s_src = FX_SRC_CNB;
static long     s_date = 0;
static uint32_t s_gen = 0;

// What the rows above are for ("CZK:EUR,USD,..."). A change in the web UI or
// an import makes it differ: the rows are dropped and asked for at once, so the
// screen never shows one currency's numbers under another's name.
static char s_sig[40] = "";

static bool          s_needPrev = false;   // CNB: the previous fixing is still to fetch
// When the source last answered and when to ask next (0.9.7). A failure is
// retried after 5 min, then 10, 20, 40, at most an hour - a source that is
// down is not hammered, and one that comes back is found within the hour.
static SrcHealth s_health;

bool        Fx_Valid()  { return s_ok; }
FxSource    Fx_Source() { return s_src; }
long        Fx_Date()   { return s_date; }
uint32_t    Fx_Gen()    { return s_gen; }
const SrcHealth& Fx_Health() { return s_health; }
// Nothing new for more than a day and a bit: the source has been down through
// at least one publication, so the rate on the screen is not today's.
bool        Fx_Stale()  { return s_ok && s_health.age() > FX_STALE_MS; }
int Fx_Rows(FxRow* out, int max) {
  int n = 0;
  for (int i = 0; i < s_n && n < max; i++) out[n++] = s_rows[i];
  return n;
}

// -----------------------------------------------------------------------------
static void settingsCheck() {
  char codes[FX_MAX][4];
  const int n = Settings_FxCodesEff(codes, FX_MAX);
  const char* home = Settings_FxHomeEff();
  char sig[sizeof(s_sig)];
  snprintf(sig, sizeof(sig), "%s:", home);
  for (int i = 0; i < n; i++) {
    strncat(sig, codes[i], sizeof(sig) - strlen(sig) - 1);
    strncat(sig, ",", sizeof(sig) - strlen(sig) - 1);
  }
  if (!strcmp(sig, s_sig)) return;
  const bool had = s_sig[0] != '\0';
  strcpy(s_sig, sig);
  strcpy(s_home, home);
  s_src = strcmp(home, "CZK") ? FX_SRC_ECB : FX_SRC_CNB;
  s_n = n;
  for (int i = 0; i < n; i++) {
    memcpy(s_rows[i].code, codes[i], 4);
    s_rows[i].amount = 1;
    s_rows[i].rate = NAN;
    s_rows[i].prev = NAN;
  }
  s_ok = false;
  s_date = 0;
  s_needPrev = false;
  s_health.reset();                    // ask at the next opportunity
  s_gen++;
  if (had) Serial.printf("KURZY: nastaveni zmeneno (%s), nacitam znovu\n", sig);
}

// -----------------------------------------------------------------------------
//  CNB
// -----------------------------------------------------------------------------
#define CNB_MAX 48                     // the sheet has about 30 lines

static bool fetchCnb() {
  String body;
  if (!Net_GetString(FX_CNB_URL, body, "KURZY")) {
    Status_Set(ST_FX, "CNB: chyba stahovani");
    return false;
  }
  static FxCnbQuote q[CNB_MAX];
  long date = 0;
  const int nq = FxParse_Cnb(body.c_str(), &date, q, CNB_MAX);
  body = String();
  if (nq <= 0) {
    Status_Set(ST_FX, "CNB: nerozumim odpovedi");
    return false;
  }
  const bool newDay = date != s_date;
  int found = 0;
  for (int i = 0; i < s_n; i++) {
    FxRow& r = s_rows[i];
    if (newDay) r.prev = NAN;
    r.rate = NAN;
    for (int k = 0; k < nq; k++) {
      if (strcmp(q[k].code, r.code)) continue;
      r.amount = q[k].amount;
      r.rate = (float)q[k].rate;
      found++;
      break;
    }
  }
  s_date = date;
  s_ok = true;
  if (newDay && date) s_needPrev = true;
  s_gen++;
  Serial.printf("KURZY: CNB %ld, %d z %d men\n", date, found, s_n);
  Status_Set(ST_FX, "OK, CNB %02ld.%02ld.%04ld, %d men",
             date % 100, date / 100 % 100, date / 10000, found);
  return true;
}

// The day before: CNB answers with the last fixing on or before it, which is
// the previous working day - exactly what the arrow compares with.
static void fetchCnbPrev() {
  const long d = FxParse_DayBefore(s_date);
  char url[200];
  snprintf(url, sizeof(url), "%s?date=%02ld.%02ld.%04ld", FX_CNB_URL,
           d % 100, d / 100 % 100, d / 10000);
  String body;
  if (!Net_GetString(url, body, "KURZY")) return;     // no arrows, nothing worse
  static FxCnbQuote q[CNB_MAX];
  long date = 0;
  const int nq = FxParse_Cnb(body.c_str(), &date, q, CNB_MAX);
  body = String();
  if (nq <= 0) return;
  for (int i = 0; i < s_n; i++) {
    FxRow& r = s_rows[i];
    for (int k = 0; k < nq; k++) {
      if (strcmp(q[k].code, r.code)) continue;
      // Same amount on both days in practice; scaled anyway, to be sure.
      r.prev = (float)(q[k].rate * r.amount / q[k].amount);
      break;
    }
  }
  s_gen++;
  Serial.printf("KURZY: CNB predchozi %ld\n", date);
}

// -----------------------------------------------------------------------------
//  ECB
// -----------------------------------------------------------------------------
static const FxEcbSeries* findSeries(const FxEcbSeries* s, int n, const char* code) {
  for (int i = 0; i < n; i++) if (!strcmp(s[i].code, code)) return &s[i];
  return nullptr;
}

// Units of code per euro, [0] newest, [1] the one before. EUR is 1 by definition.
static bool perEur(const FxEcbSeries* s, int n, const char* code, double v[2]) {
  if (!strcmp(code, "EUR")) { v[0] = v[1] = 1.0; return true; }
  const FxEcbSeries* x = findSeries(s, n, code);
  if (!x || !(x->val[0] > 0)) return false;
  v[0] = x->val[0];
  v[1] = x->date[1] ? x->val[1] : 0;
  return true;
}

static bool fetchEcb() {
  // Every series needed: the watched ones and the home one, none of them EUR.
  char keys[64] = "";
  auto addKey = [&](const char* c) {
    if (!strcmp(c, "EUR") || strstr(keys, c)) return;
    if (keys[0]) strncat(keys, "+", sizeof(keys) - strlen(keys) - 1);
    strncat(keys, c, sizeof(keys) - strlen(keys) - 1);
  };
  for (int i = 0; i < s_n; i++) addKey(s_rows[i].code);
  addKey(s_home);
  if (!keys[0]) return false;          // nothing to ask (cannot happen: home != code)

  char url[256];
  snprintf(url, sizeof(url), "%s%s%s", FX_ECB_URL, keys, FX_ECB_TAIL);
  String body;
  if (!Net_GetString(url, body, "KURZY")) {
    Status_Set(ST_FX, "ECB: chyba stahovani");
    return false;
  }
  FxEcbSeries ser[FX_MAX + 1];
  const int ns = FxParse_Ecb(body.c_str(), ser, FX_MAX + 1);
  body = String();
  if (ns <= 0) {
    Status_Set(ST_FX, "ECB: nerozumim odpovedi");
    return false;
  }

  double h[2];
  if (!perEur(ser, ns, s_home, h)) {
    Status_Set(ST_FX, "ECB: chybi %s", s_home);
    return false;
  }
  int found = 0;
  for (int i = 0; i < s_n; i++) {
    FxRow& r = s_rows[i];
    double x[2];
    r.rate = r.prev = NAN;
    if (!perEur(ser, ns, r.code, x)) continue;
    // One unit of code in the home currency: (home per EUR) / (code per EUR).
    const double unit = h[0] / x[0];
    r.amount = FxParse_Amount(unit);
    r.rate = (float)(unit * r.amount);
    if (h[1] > 0 && x[1] > 0) r.prev = (float)(h[1] / x[1] * r.amount);
    found++;
  }
  // The date of the fixing: the home series', or any when home is the euro.
  long date = 0;
  const FxEcbSeries* hs = findSeries(ser, ns, s_home);
  date = hs ? hs->date[0] : ser[0].date[0];
  s_date = date;
  s_ok = true;
  s_gen++;
  Serial.printf("KURZY: ECB %ld, %d z %d men\n", date, found, s_n);
  Status_Set(ST_FX, "OK, ECB %02ld.%02ld.%04ld, %d men",
             date % 100, date / 100 % 100, date / 10000, found);
  return true;
}

// -----------------------------------------------------------------------------
// How long until the next look. The rate moves once per working day, so:
// today's is in -> hours; not out yet -> until it is; out but not seen -> soon.
static unsigned long nextWait() {
  if (!Outside_TimeValid()) return FX_WAIT_MS;
  time_t now = time(nullptr);
  struct tm lt; localtime_r(&now, &lt);
  const long today = (long)(lt.tm_year + 1900) * 10000 + (lt.tm_mon + 1) * 100 + lt.tm_mday;
  if (s_date == today) return FX_PERIOD_MS;
  const bool workday = lt.tm_wday >= 1 && lt.tm_wday <= 5;
  if (!workday) return FX_WAIT_MS;
  const int pub = s_src == FX_SRC_CNB ? FX_CNB_PUB_MIN : FX_ECB_PUB_MIN;
  const int mins = lt.tm_hour * 60 + lt.tm_min;
  if (mins >= pub) return FX_PUB_RETRY_MS;
  const unsigned long until = (unsigned long)(pub - mins) * 60000UL;
  return until < FX_WAIT_MS ? until : FX_WAIT_MS;
}

void Fx_Tick() {
  settingsCheck();
  if (!Settings_ScreenEnabled(SCREEN_FX_I)) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (s_n == 0) return;

  if (s_needPrev) {                    // the second half of a CNB update
    s_needPrev = false;
    fetchCnbPrev();
    return;
  }
  if (s_health.tried && s_health.nextIn() > 0) return;
  const bool ok = s_src == FX_SRC_CNB ? fetchCnb() : fetchEcb();
  if (ok) s_health.result(true, nextWait());
  else    s_health.failed(FX_RETRY_MS, FX_RETRY_MAX_MS);
  s_gen++;                             // the screen shows the state, not only the data
}
