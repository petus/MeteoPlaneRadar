// =============================================================================
//  MeteoPlaneRadar
//  Cryptocurrency prices (0.9.7). See Crypto.h.
//
//  One request:
//    /simple/price?ids=bitcoin,ethereum&vs_currencies=czk
//                 &include_24hr_change=true&include_last_updated_at=true
//  answers
//    {"bitcoin":{"czk":2051234.5,"czk_24h_change":-1.41,"last_updated_at":1763050000},...}
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Crypto.h"
#include "Net.h"
#include "Settings.h"
#include "Status.h"
#include "SrcHealth.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

static CrRow     s_rows[CR_MAX];
static int       s_n = 0;
static bool      s_ok = false;
static char      s_vs[4] = "";
static time_t    s_updated = 0;
static uint32_t  s_gen = 0;
static SrcHealth s_health;
static char      s_sig[48] = "";       // "CZK:BTC,ETH," - what the rows are for

bool        Crypto_Valid()   { return s_ok; }
time_t      Crypto_Updated() { return s_updated; }
uint32_t    Crypto_Gen()     { return s_gen; }
const SrcHealth& Crypto_Health() { return s_health; }
int Crypto_Rows(CrRow* out, int max) {
  int n = 0;
  for (int i = 0; i < s_n && n < max; i++) out[n++] = s_rows[i];
  return n;
}

// A different home currency or list: drop what we hold (it is in the wrong
// currency or for other coins) and ask at once.
static void settingsCheck() {
  char codes[CR_MAX][6];
  const int n = Settings_CryptoCodesEff(codes, CR_MAX);
  const char* vs = Settings_FxHomeEff();
  char sig[sizeof(s_sig)];
  snprintf(sig, sizeof(sig), "%s:", vs);
  for (int i = 0; i < n; i++) {
    strncat(sig, codes[i], sizeof(sig) - strlen(sig) - 1);
    strncat(sig, ",", sizeof(sig) - strlen(sig) - 1);
  }
  if (!strcmp(sig, s_sig)) return;
  const bool had = s_sig[0] != '\0';
  strcpy(s_sig, sig);
  strcpy(s_vs, vs);
  s_n = n;
  for (int i = 0; i < n; i++) {
    memcpy(s_rows[i].sym, codes[i], sizeof(s_rows[i].sym));
    s_rows[i].price = NAN;
    s_rows[i].chg24 = NAN;
  }
  s_ok = false;
  s_updated = 0;
  s_health.reset();
  s_gen++;
  if (had) Serial.printf("KRYPTO: nastaveni zmeneno (%s), nacitam znovu\n", sig);
}

static bool fetch() {
  char ids[96] = "";
  for (int i = 0; i < s_n; i++) {
    const char* id = Settings_CryptoId(s_rows[i].sym);
    if (!id) continue;
    if (ids[0]) strncat(ids, ",", sizeof(ids) - strlen(ids) - 1);
    strncat(ids, id, sizeof(ids) - strlen(ids) - 1);
  }
  if (!ids[0]) return false;
  char vs[4];
  for (int i = 0; i < 4; i++) vs[i] = (char)tolower((unsigned char)s_vs[i]);

  char url[320];
  int len = snprintf(url, sizeof(url), "%s%s&vs_currencies=%s%s", CR_URL, ids, vs, CR_URL_TAIL);
  if (Settings_CgKey()[0] && len > 0 && len < (int)sizeof(url))
    snprintf(url + len, sizeof(url) - len, "&x_cg_demo_api_key=%s", Settings_CgKey());

  String body;
  if (!Net_GetString(url, body, "KRYPTO")) {
    const int code = Net_LastCode();
    if (code == 429)      Status_Set(ST_CRYPTO, "CoinGecko: prilis mnoho dotazu (429)");
    else if (code == 401) Status_Set(ST_CRYPTO, "CoinGecko: neplatny API klic (401)");
    else if (code > 0)    Status_Set(ST_CRYPTO, "CoinGecko: HTTP %d", code);
    else                  Status_Set(ST_CRYPTO, "CoinGecko: nedostupny");
    return false;
  }
  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, body);
  body = String();
  if (err || !doc.is<JsonObject>()) {
    Status_Set(ST_CRYPTO, "CoinGecko: nerozumim odpovedi");
    return false;
  }
  char chgKey[16];
  snprintf(chgKey, sizeof(chgKey), "%s_24h_change", vs);
  int found = 0;
  time_t newest = 0;
  for (int i = 0; i < s_n; i++) {
    CrRow& r = s_rows[i];
    const char* id = Settings_CryptoId(r.sym);
    JsonVariantConst o;
    if (id) o = doc.as<JsonVariantConst>()[id];
    r.price = NAN;
    r.chg24 = NAN;
    if (o.isNull()) continue;
    JsonVariantConst p = o[vs];
    if (p.is<double>() || p.is<long>()) { r.price = p.as<double>(); found++; }
    JsonVariantConst c = o[chgKey];
    if (c.is<double>() || c.is<long>()) r.chg24 = c.as<float>();
    const long t = o["last_updated_at"] | 0L;
    if (t > newest) newest = (time_t)t;
  }
  if (!found) {
    Status_Set(ST_CRYPTO, "CoinGecko: zadna cena v %s", s_vs);
    return false;
  }
  s_updated = newest;
  s_ok = true;
  Serial.printf("KRYPTO: %d z %d minci v %s\n", found, s_n, s_vs);
  Status_Set(ST_CRYPTO, "OK, %d z %d minci v %s", found, s_n, s_vs);
  return true;
}

void Crypto_Tick() {
  settingsCheck();
  if (!Settings_ScreenEnabled(SCREEN_CRYPTO_I)) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (s_n == 0) return;
  if (s_health.tried && s_health.nextIn() > 0) return;

  const bool ok = fetch();
  if (ok) s_health.result(true, CR_PERIOD_MS);
  else    s_health.failed(CR_RETRY_MS, CR_RETRY_MAX_MS);   // 2, 4, 8, 16, 30 min
  s_gen++;
}
