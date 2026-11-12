// =============================================================================
//  MeteoPlaneRadar
//  JSON cards - storage, validation and fetching. See Cards.h.
// =============================================================================
#include "Cards.h"
#include "CardPath.h"
#include "Net.h"
#include "Settings.h"
#include "Status.h"
#include <LittleFS.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#define CARDS_DIR "/cards"

// The table is close to 8 kB with every slot in use. It goes to PSRAM like the
// other large buffers, so the internal heap the TLS handshake needs is left
// alone whether or not anybody uses cards.
static Card*    s_cards = nullptr;    // CARDS_MAX slots + one of scratch
#define s_tmp   (s_cards[CARDS_MAX])  // where a card is parsed before it is kept
static int      s_n = 0;
static bool     s_fs = false;
static uint32_t s_rev = 1;

bool        Cards_FsReady()  { return s_fs; }
int         Cards_Count()    { return s_n; }
uint32_t    Cards_Revision() { return s_rev; }
const Card* Cards_Get(int i) { return (i >= 0 && i < s_n) ? &s_cards[i] : nullptr; }

// --- Validation -------------------------------------------------------------
// The id becomes a file name and a key in the web UI, so it is kept to what is
// safe in both without any escaping.
static bool idOk(const char* id) {
  const size_t n = strlen(id);
  if (n == 0 || n >= sizeof(((Card*)0)->id)) return false;
  for (size_t i = 0; i < n; i++) {
    const char c = id[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

static bool pathOk(const char* p) {
  if (!p || !*p) return false;
  CardPathSeg s;
  while (true) {
    while (*p == '.') p++;
    if (!*p) return true;
    if (!cardPathNext(&p, &s)) return false;
  }
}

// Copies a string member, refusing one that does not fit. Truncating instead
// would cut a URL or a path into something that still looks valid, and could
// split a UTF-8 character in a title.
static bool take(JsonVariantConst v, char* dst, size_t cap, bool required,
                 const char* what, char* err, size_t errCap) {
  const char* s = v | "";
  const size_t n = strlen(s);
  if (n == 0 && required) { snprintf(err, errCap, "%s is missing", what); return false; }
  if (n >= cap) { snprintf(err, errCap, "%s is too long (max %u)", what, (unsigned)(cap - 1)); return false; }
  memcpy(dst, s, n + 1);
  return true;
}

static bool parseCard(JsonObjectConst o, Card* c, char* err, size_t errCap) {
  memset(c, 0, sizeof(*c));
  if (o.isNull()) { snprintf(err, errCap, "not a JSON object"); return false; }
  if ((o["v"] | 1) != 1) { snprintf(err, errCap, "unsupported card version"); return false; }

  const char* id = o["id"] | "";
  if (!idOk(id)) { snprintf(err, errCap, "id must be 1-23 characters of a-z, 0-9, -"); return false; }
  strcpy(c->id, id);

  if (!take(o["title"], c->title, sizeof(c->title), true, "title", err, errCap)) return false;
  // A card with a "script" draws itself (Script.h). It may show the clock
  // alone and need no address, and it needs no rows.
  const char* script = o["script"] | "";
  c->hasScript = (*script != '\0');
  if (strlen(script) > SCRIPT_SRC_MAX) {
    snprintf(err, errCap, "script is longer than %d bytes", (int)SCRIPT_SRC_MAX);
    return false;
  }

  if (!take(o["url"], c->url, sizeof(c->url), !c->hasScript, "url", err, errCap)) return false;
  if (c->url[0] && strncmp(c->url, "http://", 7) != 0 && strncmp(c->url, "https://", 8) != 0) {
    snprintf(err, errCap, "url must start with http:// or https://");
    return false;
  }

  int refresh = o["refresh_min"] | CARD_REFRESH_DEF;
  c->refreshMin = (uint16_t)constrain(refresh, CARD_REFRESH_MIN, 1440);

  JsonArrayConst rows = o["rows"];
  if (rows.size() > CARD_ROWS_MAX || (!c->hasScript && (rows.isNull() || rows.size() == 0))) {
    snprintf(err, errCap, "rows must hold 1 to %d items", CARD_ROWS_MAX);
    return false;
  }
  for (JsonObjectConst r : rows) {
    CardRow* row = &c->rows[c->nRows];
    if (!take(r["path"], row->path, sizeof(row->path), true, "row path", err, errCap)) return false;
    if (!pathOk(row->path)) { snprintf(err, errCap, "row path is malformed: %s", row->path); return false; }
    if (!take(r["label"], row->label, sizeof(row->label), false, "row label", err, errCap)) return false;
    if (!take(r["unit"], row->unit, sizeof(row->unit), false, "row unit", err, errCap)) return false;
    JsonVariantConst dec = r["decimals"];
    row->decimals = dec.isNull() ? -1 : (int8_t)constrain(dec.as<int>(), 0, 6);
    row->scale = r["scale"] | 1.0f;
    c->nRows++;
  }

  if (c->hasScript) {
    c->redrawS = (uint16_t)constrain((int)(o["redraw_s"] | 0), 0, 3600);
    JsonArrayConst keep = o["keep"];
    if (keep.size() > CARD_KEEP_MAX) {
      snprintf(err, errCap, "keep must hold at most %d paths", CARD_KEEP_MAX);
      return false;
    }
    for (JsonVariantConst k : keep) {
      char* dst = c->keep[c->nKeep];
      if (!take(k, dst, sizeof(c->keep[0]), true, "keep path", err, errCap)) return false;
      if (!pathOk(dst)) { snprintf(err, errCap, "keep path is malformed: %s", dst); return false; }
      c->nKeep++;
    }
  }

  JsonObjectConst note = o["note"];
  if (!note.isNull()) {
    if (!take(note["path"], c->notePath, sizeof(c->notePath), true, "note path", err, errCap)) return false;
    if (!pathOk(c->notePath)) { snprintf(err, errCap, "note path is malformed"); return false; }
    if (!take(note["label"], c->noteLabel, sizeof(c->noteLabel), false, "note label", err, errCap)) return false;
  }
  return true;
}

// --- The table --------------------------------------------------------------
// Kept sorted by id, so the order on the display does not depend on the order
// the filesystem happens to list its directory in.
static uint32_t s_installNo = 0;

static void freeData(Card* c) {
  if (c->data) { free(c->data); c->data = nullptr; }
}

// A script that was switched off (Script.h) gets a fresh start when its card
// is installed again or removed. The marker is a file next to the card.
static void clearOffMarker(const char* id) {
  char p[48];
  snprintf(p, sizeof(p), CARDS_DIR "/%s.off", id);
  if (LittleFS.exists(p)) LittleFS.remove(p);
}

static void insertSorted(const Card* c) {
  int at = 0;
  while (at < s_n && strcmp(s_cards[at].id, c->id) < 0) at++;
  if (at < s_n && strcmp(s_cards[at].id, c->id) == 0) {
    freeData(&s_cards[at]);
    s_cards[at] = *c;                               // same id: replace
    s_cards[at].installNo = ++s_installNo;
  } else {
    if (s_n >= CARDS_MAX) return;
    memmove(&s_cards[at + 1], &s_cards[at], (size_t)(s_n - at) * sizeof(Card));
    s_cards[at] = *c;
    s_cards[at].installNo = ++s_installNo;
    s_n++;
  }
  s_rev++;
}

static int indexOf(const char* id) {
  for (int i = 0; i < s_n; i++) if (strcmp(s_cards[i].id, id) == 0) return i;
  return -1;
}

const char* Cards_Data(const Card* c) { return (c && c->data && c->dataRev) ? c->data : nullptr; }
Card* Cards_Mutable(const char* id) {
  const int i = id ? indexOf(id) : -1;
  return i < 0 ? nullptr : &s_cards[i];
}

static void filePath(char* out, size_t cap, const char* id) {
  snprintf(out, cap, CARDS_DIR "/%s.json", id);
}

static void loadAll() {
  s_n = 0;
  File dir = LittleFS.open(CARDS_DIR);
  if (!dir || !dir.isDirectory()) return;

  Card& tmp = s_tmp;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const String name = f.name();
    if (f.isDirectory() || !name.endsWith(".json")) continue;
    if (f.size() == 0 || f.size() > CARD_FILE_MAX) {
      Serial.printf("Karty: %s ma %u B, preskakuji\n", name.c_str(), (unsigned)f.size());
      continue;
    }
    JsonDocument doc;
    char err[64];
    if (deserializeJson(doc, f) || !parseCard(doc.as<JsonObjectConst>(), &tmp, err, sizeof(err))) {
      Serial.printf("Karty: %s nejde nacist, preskakuji\n", name.c_str());
      continue;
    }
    insertSorted(&tmp);
  }
  Serial.printf("Karty: nacteno %d\n", s_n);
}

void Cards_Begin() {
  s_cards = (Card*)heap_caps_calloc(CARDS_MAX + 1, sizeof(Card), MALLOC_CAP_SPIRAM);
  if (!s_cards) s_cards = (Card*)calloc(CARDS_MAX + 1, sizeof(Card));
  if (!s_cards) { Serial.println("Karty: neni pamet"); return; }

  // true = format a partition that has never held a filesystem. That happens
  // once in a device's life and only writes the two superblocks. setup() calls
  // this before the panel is started, so the flash write cannot disturb a
  // frame the way one during loop() would.
  s_fs = LittleFS.begin(true);
  if (!s_fs) { Serial.println("Karty: LittleFS se nepodarilo pripojit"); return; }
  if (!LittleFS.exists(CARDS_DIR)) LittleFS.mkdir(CARDS_DIR);
  loadAll();
}

bool Cards_Install(JsonObjectConst card, char* err, size_t errCap) {
  if (!s_fs || !s_cards) { snprintf(err, errCap, "storage is not available"); return false; }

  Card& tmp = s_tmp;
  if (!parseCard(card, &tmp, err, errCap)) return false;

  if (indexOf(tmp.id) < 0 && s_n >= CARDS_MAX) {
    snprintf(err, errCap, "card limit reached (%d)", CARDS_MAX);
    return false;
  }
  if (measureJson(card) > CARD_FILE_MAX) {
    snprintf(err, errCap, "card is larger than %d bytes", CARD_FILE_MAX);
    return false;
  }

  char path[48];
  filePath(path, sizeof(path), tmp.id);
  File f = LittleFS.open(path, "w");
  if (!f) { snprintf(err, errCap, "cannot write the file"); return false; }
  const size_t wrote = serializeJson(card, f);
  f.close();
  if (wrote == 0) {
    LittleFS.remove(path);
    snprintf(err, errCap, "cannot write the file");
    return false;
  }

  clearOffMarker(tmp.id);
  insertSorted(&tmp);        // values start empty, so the next tick fetches it
  Serial.printf("Karty: ulozena %s\n", tmp.id);
  return true;
}

bool Cards_Remove(const char* id) {
  if (!s_fs || !id || !idOk(id)) return false;
  const int i = indexOf(id);
  if (i < 0) return false;

  char path[48];
  filePath(path, sizeof(path), id);
  LittleFS.remove(path);
  clearOffMarker(id);
  freeData(&s_cards[i]);
  memmove(&s_cards[i], &s_cards[i + 1], (size_t)(s_n - i - 1) * sizeof(Card));
  s_n--;
  s_rev++;
  Serial.printf("Karty: odstranena %s\n", id);
  return true;
}

// --- Fetching ---------------------------------------------------------------
// Copies at most cap-1 bytes without leaving half of a UTF-8 character behind.
static void copyUtf8(char* dst, size_t cap, const char* src) {
  size_t n = strlen(src);
  if (n >= cap) {
    n = cap - 1;
    while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

// A value as text. Numbers get the card's decimals and scale; so do strings
// that are really numbers, which several APIs send ("97123.50"). Anything
// else is shown as it came.
static bool formatValue(JsonVariantConst v, int8_t decimals, float scale,
                        char* out, size_t cap, bool* numeric = nullptr) {
  if (numeric) *numeric = false;
  if (v.isNull() || v.is<JsonArrayConst>() || v.is<JsonObjectConst>()) return false;

  bool isNum = v.is<double>();
  bool isInt = v.is<long long>();
  double x = isNum ? v.as<double>() : 0;

  if (!isNum && v.is<const char*>()) {
    const char* s = v.as<const char*>();
    char* end = nullptr;
    const double parsed = strtod(s, &end);
    if (end != s && *end == '\0' && (decimals >= 0 || scale != 1.0f)) {
      isNum = true; x = parsed;
    } else {
      copyUtf8(out, cap, s);
      return true;
    }
  }
  if (!isNum) {
    snprintf(out, cap, "%s", v.as<bool>() ? "true" : "false");
    return true;
  }

  int dec = decimals;
  if (dec < 0) dec = (isInt && scale == 1.0f) ? 0 : 2;
  snprintf(out, cap, "%.*f", dec, x * (double)scale);
  if (numeric) *numeric = true;
  return true;
}

static bool fetch(Card* c) {
  String body;
  if (!Net_GetString(c->url, body, "CARD")) {
    Status_Set(ST_CARDS, "%s: chyba stahovani", c->id);
    return false;
  }

  // Keep only what the card reads. A path the filter cannot express would be
  // a bug in CardPath.h, not in the card (it was validated on install) - but
  // parsing everything is the safe answer either way.
  JsonDocument filter;
  bool filtered = true;
  for (uint8_t i = 0; i < c->nRows; i++)
    filtered = CardPath_AddToFilter(filter, c->rows[i].path) && filtered;
  if (c->notePath[0])
    filtered = CardPath_AddToFilter(filter, c->notePath) && filtered;
  for (uint8_t i = 0; i < c->nKeep; i++)
    filtered = CardPath_AddToFilter(filter, c->keep[i]) && filtered;
  // A script card that names no path at all gets the whole answer.
  const bool anyPath = c->nRows || c->notePath[0] || c->nKeep;
  if (!anyPath) filtered = false;

  JsonDocument doc;
  DeserializationError e = filtered
      ? deserializeJson(doc, body, DeserializationOption::Filter(filter))
      : deserializeJson(doc, body);
  if (e) {
    Serial.printf("CARD: %s JSON %s\n", c->id, e.c_str());
    Status_Set(ST_CARDS, "%s: chybny JSON", c->id);
    return false;
  }

  // What the script will be handed: the parts the card's paths keep, or the
  // answer as it came. Either way it has to fit, and it is stored as text -
  // the interpreter turns it into its own objects when it needs them.
  if (c->hasScript) {
    const size_t need = anyPath ? measureJson(doc) : body.length();
    if (need > SCRIPT_DATA_MAX) {
      Status_Set(ST_CARDS, "%s: odpoved %u B, skript unese %d", c->id, (unsigned)need, (int)SCRIPT_DATA_MAX);
      return false;
    }
    if (!c->data) c->data = (char*)heap_caps_malloc(SCRIPT_DATA_MAX + 1, MALLOC_CAP_SPIRAM);
    if (!c->data) c->data = (char*)malloc(SCRIPT_DATA_MAX + 1);
    if (!c->data) { Status_Set(ST_CARDS, "%s: neni pamet", c->id); return false; }
    if (anyPath) serializeJson(doc, c->data, SCRIPT_DATA_MAX + 1);
    else         memcpy(c->data, body.c_str(), need + 1);
    c->dataRev++;
  }
  body = String();

  // Into a scratch copy first: a fetch that finds nothing must not wipe the
  // values that are on the screen.
  char val[CARD_ROWS_MAX][sizeof(((CardRow*)0)->value)];
  bool num[CARD_ROWS_MAX];
  const JsonVariantConst root = doc.as<JsonVariantConst>();
  int found = 0;
  for (uint8_t i = 0; i < c->nRows; i++) {
    const CardRow& r = c->rows[i];
    val[i][0] = '\0';
    num[i] = false;
    if (formatValue(CardPath_Resolve(root, r.path), r.decimals, r.scale, val[i], sizeof(val[i]), &num[i]))
      found++;
  }
  if (found == 0 && c->nRows > 0) {
    Status_Set(ST_CARDS, "%s: hodnoty v odpovedi nejsou", c->id);
    return false;
  }

  for (uint8_t i = 0; i < c->nRows; i++) {
    strcpy(c->rows[i].value, val[i]);
    c->rows[i].numeric = num[i];
  }
  c->note[0] = '\0';
  if (c->notePath[0])
    formatValue(CardPath_Resolve(root, c->notePath), -1, 1.0f, c->note, sizeof(c->note));

  Status_Set(ST_CARDS, "%s: OK, %d/%d hodnot", c->id, found, (int)c->nRows);
  return true;
}

void Cards_Tick() {
  if (!s_fs || s_n == 0) return;
  if (WiFi.status() != WL_CONNECTED) return;
  if (!Settings_ScreenEnabled(SCREEN_CARDS_I)) return;

  const unsigned long now = millis();
  for (int i = 0; i < s_n; i++) {
    Card* c = &s_cards[i];
    if (!c->url[0]) continue;                      // a script card without an address
    const unsigned long period = (c->valid && !c->stale)
        ? (unsigned long)c->refreshMin * 60000UL : CARD_RETRY_MS;
    if (c->everTried && now - c->lastTry < period) continue;

    c->everTried = true;
    c->lastTry = now;
    if (fetch(c)) { c->valid = true; c->stale = false; c->lastOk = now; }
    else          { c->stale = true; }
    s_rev++;
    return;                          // one request per tick, never two in a row
  }
}
