// =============================================================================
//  MeteoPlaneRadar
//  Spot electricity price (OTE via spotovaelektrina.cz) and the Czech
//  generation mix (ENTSO-E via the Fraunhofer ISE energy-charts API).
//
//  Free, no key, no registration - see the long note in Config.h for what each
//  endpoint answers and why the mix is asked for a few hours at a time rather
//  than a whole day.
//
//  TWO sources in one module because they are one subject: the price screen
//  says what electricity costs right now and the mix screen says why. They are
//  fetched independently though - one being down never blanks the other.
//
//  WHAT THIS MODULE DOES NOT DO: it never decides what the user actually pays.
//  The spot price is the exchange price for the energy alone; distribution,
//  the regulated components and VAT are all on top of it and differ by supplier
//  and region. Settings_PriceFee() and Settings_PriceVat() hold what the user
//  told us about their own tariff, and Energy_FinalCzkKwh() applies exactly
//  that and nothing else. With both at zero the screens show the bare exchange
//  price, which is the honest default: a made-up surcharge would be worse than
//  none, because it would look like the real bill.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>
#include <time.h>
#include "Config.h"

// --- Spot price -------------------------------------------------------------
//
// Prices are held in CZK per MEGAWATT hour, exactly as the source gives them.
//
// A day is 96 QUARTER HOURS, not 24 hours - that is the block OTE actually
// trades and bills in. Index 0 is 00:00-00:15, index 95 is 23:45-24:00, so
// slot = hour * 4 + minute / 15.
#define PRICE_SLOTS      96
#define PRICE_PER_HOUR    4

// Helpers, so no caller has to remember which way the division goes.
static inline int Price_SlotOf(int hour, int minute) {
  return hour * PRICE_PER_HOUR + minute / (60 / PRICE_PER_HOUR);
}
static inline int Price_SlotHour(int slot)   { return slot / PRICE_PER_HOUR; }
static inline int Price_SlotMinute(int slot) { return (slot % PRICE_PER_HOUR) * (60 / PRICE_PER_HOUR); }

// Call from loop(). Fetches when due, cheap no-op otherwise. One request per
// call at most, so the two sources can never queue up behind each other.
void Energy_Tick();

// --- Does either screen mean anything where this device is? ------------------
//
// Neither source is global. The price is the Czech market in CZK and there is
// no version of it for someone in Texas; the mix works wherever energy-charts
// has a country, which is Europe. Both screens ask before drawing, and the
// fetchers ask before spending a request on an API that cannot answer.
//
// The boxes are in Config.h, along with the reasoning for using boxes at all.
bool Energy_PriceInArea();
bool Energy_MixInArea();

// Ask for a refresh on the next tick (after a settings change).
void Energy_Invalidate();

// 0.9.7: how the two sources are doing (see SrcHealth.h).
struct SrcHealth;
const SrcHealth& Energy_PriceHealth();
const SrcHealth& Energy_MixHealth();

bool Energy_PriceValid();          // today's prices are in
bool Energy_TomorrowValid();       // ...and tomorrow's, which arrive after ~14:00

// CZK/MWh for slot 0..95. NAN when that slot is not known.
float Energy_PriceToday(int slot);
float Energy_PriceTomorrow(int slot);

// The price for the quarter hour the clock is in right now, CZK/MWh. NAN when
// unknown. This is what a price site means by "the current price" and what the
// bill is worked out from - NOT the average of the hour it sits in.
float Energy_PriceNow();

// Cheapest / priciest slot of a day, -1 when that day is not loaded.
// day 0 = today, 1 = tomorrow.
int Energy_CheapestSlot(int day);
int Energy_PriceHighestSlot(int day);

// Lowest and highest of a day, for the colour scale. Returns false when the day
// is not loaded.
bool Energy_DayRange(int day, float* lo, float* hi);

// Apply the user's own tariff: (spot + fee) * (1 + VAT), converted to CZK per
// KILOWATT hour - the unit people actually think in.
float Energy_FinalCzkKwh(float spotCzkMwh);

// --- Generation mix ---------------------------------------------------------
//
// The twenty production types the API reports are folded into the six groups a
// 480 px ring can actually label. The order here is the order they are drawn
// in, which is deliberately stable: a ring whose segments jump around between
// refreshes is unreadable even when every number on it is right.
enum MixGroup : uint8_t {
  MIX_NUCLEAR = 0,
  MIX_COAL,
  MIX_GAS,
  MIX_SOLAR,
  MIX_WIND,
  MIX_HYDRO,
  MIX_BIOMASS,
  MIX_OTHER,
  MIX_GROUPS
};

bool  Energy_MixValid();
float Energy_MixMw(uint8_t group);     // MW in that group, 0 when unknown
float Energy_MixTotalMw();             // sum of the groups above
float Energy_MixShare(uint8_t group);  // per cent of the total, 0..100

// Share of generation that is renewable, as the source itself reports it
// (which is not quite the sum of our groups - it counts waste partly, and we
// deliberately do not). -1 when the source did not send it.
float Energy_RenewableShare();

// Load in MW, and the cross-border balance: POSITIVE means the country is
// exporting, negative means it is importing. NAN when unknown.
float Energy_LoadMw();
float Energy_ExportMw();

// When the newest sample the mix was taken from was measured (UTC epoch), so
// the screen can say how old it is rather than implying it is live. 0 = unknown.
time_t Energy_MixSampleTime();

// Group name in the interface language, ASCII for the panel.
const char* Energy_GroupName(uint8_t group);
