// =============================================================================
//  MeteoPlaneRadar
//  JSON cards - the path that picks one value out of a downloaded document.
//
//  A card says WHERE in the answer its number is, in a small dotted notation:
//
//      data.amount                       object members
//      list[0].price                     array element by position
//      rates[currencyCode=EUR].rate      array element by content: the first
//                                        one whose "currencyCode" is "EUR"
//
//  The third form is the reason this is not just a list of keys. Real APIs
//  return arrays whose order is nobody's promise (the CNB exchange-rate list is
//  one), and a card that said "rates[7]" would show the wrong currency the day
//  one is added.
//
//  CardPath_AddToFilter() turns the same path into an ArduinoJson filter, so
//  only the members the card actually reads are kept while parsing. A 6 kB
//  answer of 31 currencies then costs the heap two numbers and a date instead
//  of the whole table.
//
//  Nothing here touches Arduino.h, so the file compiles on a PC and is tested
//  there (tools/test_cardpath.cpp).
// =============================================================================
#pragma once
#include <ArduinoJson.h>
#include <stdlib.h>
#include <string.h>

#define CARDPATH_SEG_MAX 40     // longest single key / filter inside a path

// One step of a path, cut out of the string by cardPathNext().
struct CardPathSeg {
  enum Kind : uint8_t { KEY, INDEX, MATCH } kind;
  char a[CARDPATH_SEG_MAX];     // KEY: member name    MATCH: member to compare
  char b[CARDPATH_SEG_MAX];     // MATCH: value it must have
  int  index;                   // INDEX
};

// Reads the next step at *pp and advances it. false = end of path or a step
// that is malformed / too long (the caller treats both as "no value").
static inline bool cardPathNext(const char** pp, CardPathSeg* s) {
  const char* p = *pp;
  while (*p == '.') p++;
  if (!*p) return false;

  if (*p == '[') {
    p++;
    const char* end = strchr(p, ']');
    if (!end) return false;
    const char* eq = (const char*)memchr(p, '=', (size_t)(end - p));
    if (eq) {
      const size_t la = (size_t)(eq - p), lb = (size_t)(end - eq - 1);
      if (la == 0 || la >= CARDPATH_SEG_MAX || lb >= CARDPATH_SEG_MAX) return false;
      memcpy(s->a, p, la);      s->a[la] = '\0';
      memcpy(s->b, eq + 1, lb); s->b[lb] = '\0';
      s->kind = CardPathSeg::MATCH;
    } else {
      if (end == p) return false;
      for (const char* q = p; q < end; q++) if (*q < '0' || *q > '9') return false;
      s->index = atoi(p);
      s->kind = CardPathSeg::INDEX;
    }
    *pp = end + 1;
    return true;
  }

  size_t n = 0;
  while (p[n] && p[n] != '.' && p[n] != '[') n++;
  if (n == 0 || n >= CARDPATH_SEG_MAX) return false;
  memcpy(s->a, p, n); s->a[n] = '\0';
  s->kind = CardPathSeg::KEY;
  *pp = p + n;
  return true;
}

// Does this member equal the text written in the path? Numbers compare by
// their printed form, so [id=5] finds both 5 and "5".
static inline bool cardPathEquals(JsonVariantConst v, const char* want) {
  if (v.is<const char*>()) return strcmp(v.as<const char*>(), want) == 0;
  if (v.is<bool>())        return strcmp(v.as<bool>() ? "true" : "false", want) == 0;
  if (v.is<long long>()) {
    char* end = nullptr;
    const long long w = strtoll(want, &end, 10);
    return end && *end == '\0' && end != want && v.as<long long>() == w;
  }
  if (v.is<double>()) {
    char* end = nullptr;
    const double w = strtod(want, &end);
    return end && *end == '\0' && end != want && v.as<double>() == w;
  }
  return false;
}

// The value the path points at, or an unbound (null) variant.
static inline JsonVariantConst CardPath_Resolve(JsonVariantConst root, const char* path) {
  JsonVariantConst cur = root;
  if (!path) return JsonVariantConst();
  const char* p = path;
  CardPathSeg s;
  while (true) {
    while (*p == '.') p++;
    if (!*p) return cur;
    if (!cardPathNext(&p, &s)) return JsonVariantConst();

    if (s.kind == CardPathSeg::KEY) {
      cur = cur[s.a];
    } else if (s.kind == CardPathSeg::INDEX) {
      cur = cur[(size_t)s.index];
    } else {
      JsonVariantConst hit;
      for (JsonVariantConst el : cur.as<JsonArrayConst>()) {
        if (cardPathEquals(el[s.a], s.b)) { hit = el; break; }
      }
      cur = hit;
    }
    if (cur.isNull()) return JsonVariantConst();
  }
}

// Adds what this path reads to an ArduinoJson filter document. Several paths
// may be added to the same filter. Returns false for a malformed path - the
// caller should then parse without a filter rather than lose the value.
static inline bool CardPath_AddToFilter(JsonDocument& filter, const char* path) {
  if (!path || !*path) return false;
  JsonVariant cur = filter.isNull() ? filter.to<JsonVariant>() : filter.as<JsonVariant>();
  const char* p = path;
  CardPathSeg s;
  while (true) {
    while (*p == '.') p++;
    if (!*p) break;
    if (!cardPathNext(&p, &s)) return false;
    if (cur.is<bool>()) return true;          // an earlier path already keeps it all

    if (s.kind == CardPathSeg::KEY) {
      JsonObject o = cur.is<JsonObject>() ? cur.as<JsonObject>() : cur.to<JsonObject>();
      cur = o[s.a].isNull() ? o[s.a].to<JsonVariant>() : o[s.a].as<JsonVariant>();
    } else {
      // In a filter one array element stands for all of them, whichever index
      // or match the path asked for.
      JsonArray a = cur.is<JsonArray>() ? cur.as<JsonArray>() : cur.to<JsonArray>();
      JsonVariant el = a.size() ? a[0].as<JsonVariant>() : a.add<JsonVariant>();
      if (s.kind == CardPathSeg::MATCH && !el.is<bool>()) {
        JsonObject eo = el.is<JsonObject>() ? el.as<JsonObject>() : el.to<JsonObject>();
        if (eo[s.a].isNull()) eo[s.a] = true;   // the member being compared
      }
      cur = el;
    }
  }
  // A leaf that an earlier, longer path already turned into a container stays
  // one - widening it to "true" here would only cost memory.
  if (cur.isNull()) cur.set(true);
  return true;
}
