// Host test for MeteoPlaneRadar/CardPath.h. No device needed:
//
//   g++ -std=c++17 -I <ArduinoJson>/src -I MeteoPlaneRadar tools/test_cardpath.cpp -o /tmp/t && /tmp/t
//
// The sample is the shape of https://api.cnb.cz/cnbapi/exrates/daily, with the
// currencies deliberately out of any useful order.
#include "CardPath.h"
#include <cstdio>
#include <string>
#include <cmath>

// ArduinoJson's number parser can be one ulp off the compiler's, so compare loosely.
static bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const char* CNB = R"({"rates":[
 {"validFor":"2026-11-11","order":218,"country":"Australia","currency":"dollar","amount":1,"currencyCode":"AUD","rate":13.612},
 {"validFor":"2026-11-11","order":218,"country":"USA","currency":"dollar","amount":1,"currencyCode":"USD","rate":21.049},
 {"validFor":"2026-11-11","order":218,"country":"Japan","currency":"yen","amount":100,"currencyCode":"JPY","rate":13.741},
 {"validFor":"2026-11-11","order":218,"country":"EMU","currency":"euro","amount":1,"currencyCode":"EUR","rate":24.345}
]})";

int main() {
  JsonDocument doc;
  CHECK(!deserializeJson(doc, CNB));
  JsonVariantConst r = doc.as<JsonVariantConst>();

  CHECK(near(CardPath_Resolve(r, "rates[currencyCode=EUR].rate").as<double>(), 24.345));
  CHECK(near(CardPath_Resolve(r, "rates[currencyCode=USD].rate").as<double>(), 21.049));
  CHECK(std::string(CardPath_Resolve(r, "rates[0].validFor").as<const char*>()) == "2026-11-11");
  CHECK(CardPath_Resolve(r, "rates[amount=100].currencyCode").as<std::string>() == "JPY");   // numeric match
  CHECK(CardPath_Resolve(r, "rates[3].currencyCode").as<std::string>() == "EUR");
  CHECK(near(CardPath_Resolve(r, ".rates.[1].rate").as<double>(), 21.049));                      // stray dots tolerated

  // Things that must come back empty rather than crash or guess.
  CHECK(CardPath_Resolve(r, "rates[currencyCode=XXX].rate").isNull());
  CHECK(CardPath_Resolve(r, "rates[99].rate").isNull());
  CHECK(CardPath_Resolve(r, "nothing.here").isNull());
  CHECK(CardPath_Resolve(r, "rates[currencyCode=EUR").isNull());      // unterminated
  CHECK(CardPath_Resolve(r, "rates[abc].rate").isNull());             // not an index
  CHECK(CardPath_Resolve(r, "rates[=EUR].rate").isNull());
  CHECK(CardPath_Resolve(r, "rates.rate").isNull());                  // key on an array
  CHECK(CardPath_Resolve(r, nullptr).isNull());

  // The filter keeps exactly what the three paths read.
  JsonDocument filter;
  CHECK(CardPath_AddToFilter(filter, "rates[currencyCode=EUR].rate"));
  CHECK(CardPath_AddToFilter(filter, "rates[currencyCode=USD].rate"));
  CHECK(CardPath_AddToFilter(filter, "rates[0].validFor"));
  std::string fs; serializeJson(filter, fs);
  printf("filter: %s\n", fs.c_str());
  CHECK(fs == R"({"rates":[{"currencyCode":true,"rate":true,"validFor":true}]})");

  JsonDocument small;
  CHECK(!deserializeJson(small, CNB, DeserializationOption::Filter(filter)));
  JsonVariantConst s = small.as<JsonVariantConst>();
  CHECK(near(CardPath_Resolve(s, "rates[currencyCode=EUR].rate").as<double>(), 24.345));
  CHECK(near(CardPath_Resolve(s, "rates[currencyCode=USD].rate").as<double>(), 21.049));
  CHECK(std::string(CardPath_Resolve(s, "rates[0].validFor").as<const char*>()) == "2026-11-11");
  CHECK(s["rates"][0]["country"].isNull());                           // dropped
  std::string full, kept; serializeJson(doc, full); serializeJson(small, kept);
  printf("document %zu B -> filtered %zu B\n", full.size(), kept.size());

  // Other shapes: a plain member, a root array, a path into an already-kept subtree.
  JsonDocument d2; deserializeJson(d2, R"({"data":{"amount":"97123.5","base":"BTC"}})");
  CHECK(CardPath_Resolve(d2.as<JsonVariantConst>(), "data.amount").as<std::string>() == "97123.5");
  JsonDocument d3; deserializeJson(d3, R"([{"id":5,"v":1.5},{"id":7,"v":2.5}])");
  CHECK(near(CardPath_Resolve(d3.as<JsonVariantConst>(), "[id=7].v").as<double>(), 2.5));
  CHECK(near(CardPath_Resolve(d3.as<JsonVariantConst>(), "[1].v").as<double>(), 2.5));

  JsonDocument f2;
  CHECK(CardPath_AddToFilter(f2, "data"));
  CHECK(CardPath_AddToFilter(f2, "data.amount"));     // already covered by "data"
  std::string f2s; serializeJson(f2, f2s);
  CHECK(f2s == R"({"data":true})");
  JsonDocument f3;
  CHECK(CardPath_AddToFilter(f3, "[id=7].v"));
  std::string f3s; serializeJson(f3, f3s);
  CHECK(f3s == R"([{"id":true,"v":true}])");
  JsonDocument f4;
  CHECK(!CardPath_AddToFilter(f4, "a[b"));
  CHECK(!CardPath_AddToFilter(f4, ""));

  printf(fails ? "%d FAILED\n" : "all passed\n", fails);
  return fails ? 1 : 0;
}
