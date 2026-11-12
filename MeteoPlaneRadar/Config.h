// =============================================================================
//  MeteoPlaneRadar
//  Config.h - ALL user-tunable settings in one place.
//
//  This is the only file you normally need to touch when adapting the project:
//  time zone, default location, ranges, poll intervals, AP name, limits.
//  Everything here is a compile-time default; the location, brightness, units,
//  last screen and last range are also stored in NVS at runtime (Settings.*).
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
//  Boards:  Waveshare ESP32-S3-Touch-LCD-2.1  and  ESP32-S3-Touch-LCD-2.8C
//           (both round 480x480, ST7701; told apart at boot - see BOARD_FORCE)
// =============================================================================
#pragma once
#include "Version.h"   // FW_VERSION - jde do hlavicky User-Agent nize

// ---------------------------------------------------------------------------
//  Board pins / bus
// ---------------------------------------------------------------------------
#define I2C_SDA   15
#define I2C_SCL   7
#define BOOT_PIN  0        // hold at power-up (~3 s) = factory reset

// ---------------------------------------------------------------------------
//  Which board?
//
//  The 2.1 and the 2.8C are pin-for-pin identical - same RGB lines, same I2C,
//  same expander, same backlight - and differ only in the ST7701 register
//  sequence, the vertical timing and the touch controller. So one binary runs
//  on both and works out which it is at boot, by asking the touch controller
//  who it is (CST820 at 0x15 = 2.1, GT911 at 0x5D = 2.8C). See Board.h.
//
//  0  = detect at boot (leave it here)
//  21 = always the 2.1
//  28 = always the 2.8C
//
//  Force it only if the detection gets it wrong on your board - a dead touch
//  controller would otherwise leave you on BOARD_FALLBACK, which for the wrong
//  board means a rolling or blank picture rather than merely no touch.
// ---------------------------------------------------------------------------
#define BOARD_FORCE 0

// Which board to assume when nothing answers on I2C. The 2.1 is the original
// target and by far the more common of the two.
#define BOARD_FALLBACK BOARD_LCD_2_1

// ---------------------------------------------------------------------------
//  Time zone (POSIX TZ string)
//
//  There is no NTP client and no system clock - see the note above setup() in
//  the .ino. This string is still needed: CHMU.cpp turns each frame's UTC
//  timestamp into local time through localtime_r(), and these rules are what
//  decide CET or CEST for the date of that particular frame.
// ---------------------------------------------------------------------------
#define TZ_INFO   "CET-1CEST,M3.5.0,M10.5.0/3"

// ---------------------------------------------------------------------------
//  Default location (Prague). Overwritten on first boot by IP geolocation, or
//  manually in the WiFi portal; the stored value always wins.
// ---------------------------------------------------------------------------
#define DEFAULT_LAT 50.0755
#define DEFAULT_LON 14.4378

// ---------------------------------------------------------------------------
//  Configuration access point (WiFi portal and OTA share this name)
// ---------------------------------------------------------------------------
#define AP_SSID     "MeteoPlaneRadar"
#define AP_PASSWORD ""     // "" = open network

// ---------------------------------------------------------------------------
//  Aircraft radar (adsb.fi)
// ---------------------------------------------------------------------------
#define ADSB_MAX 100       // max aircraft held/drawn (airborne only)

// Selectable ranges in km. Keep them ascending; the count is derived.
#define PLANE_RANGES_KM { 10.0f, 25.0f, 50.0f, 100.0f }

// Poll interval by range - larger areas return more data and are less
// time-critical, so they are polled less often (easier on the free API).
// After a failed fetch the interval is doubled.
#define ADSB_PERIOD_NEAR_MS  5000    // up to  ADSB_NEAR_KM
#define ADSB_PERIOD_MID_MS  10000    // up to  ADSB_MID_KM
#define ADSB_PERIOD_FAR_MS  15000    // beyond ADSB_MID_KM
#define ADSB_NEAR_KM 25.0f
#define ADSB_MID_KM  50.0f

// ---------------------------------------------------------------------------
//  Weather radar (CHMU)
// ---------------------------------------------------------------------------
// Radius in km around the user's position. The value 0 is special: it means
// "the whole country", a fixed view that ignores where the user is (see the
// CZ_VIEW_* box below). Keep it last - it is the widest of them all.
#define METEO_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f, 0.0f }

// Prodleva mezi pokusy o data meteoradaru, kdyz zadna nejsou. Plati pro oba
// zdroje: u CHMU bez ni slo prvni nacteni znovu pri kazdem pruchodu smyckou,
// tedy cely vypis kazdou vterinu; u RainVieweru se naopak po neuspechu nezkusilo
// nic a radar zustal prazdny az do zmeny dosahu nebo restartu.
#define RADAR_RETRY_MS 60000UL

// The fixed "whole country" view: centre of the republic and a radius wide
// enough to hold it. Expressed as centre + radius rather than a bounding box on
// purpose - the CHMU image has a different pixel-per-degree scale on each axis,
// so a box that looks square in pixels is NOT square on the ground and would
// stretch the country vertically. Going through the same radius maths as every
// other range keeps the scale honest (1.08 km per pixel in both directions).
//
// The country is 486 x 279 km, so 260 km of radius covers it with a margin that
// keeps the western tip clear of the round bezel. Costs ~480 kB of PSRAM per
// frame, ~2.9 MB for all six.
#define CZ_VIEW_LAT       49.805f
#define CZ_VIEW_LON       15.475f
#define CZ_VIEW_RADIUS_KM 260.0f

// ---------------------------------------------------------------------------
//  Clock and outside temperature (the line under the screen dots)
// ---------------------------------------------------------------------------
// Temperature comes from Open-Meteo: free, no key, no registration, a few
// hundred bytes per answer.
#define OUTSIDE_TEMP_URL "https://api.open-meteo.com/v1/forecast"
#define OUTSIDE_TEMP_PERIOD_MS 600000UL   // 10 min - it is a model value
#define OUTSIDE_TEMP_RETRY_MS   60000UL   // sooner while we have nothing yet

// ---------------------------------------------------------------------------
//  Degree symbol
//
//  The font DOES have one. Up to 0.7.4 this said it did not, and spelled the
//  unit "degC" to be safe - because the byte it tried was the wrong one.
//
//  The built-in font is not 7-bit ASCII, it is the 256-character GLCD table
//  (CP437-ish), and in it:
//
//    0xB0  is a checkerboard dither pattern  - what this used to ask for, and
//          exactly the "random glyph" it warned about
//    0xF8  is a proper degree ring, drawn in the top-left of the cell, which
//          is where a degree sign belongs
//
//  Two things that make this safe rather than a gamble. Arduino_GFX indexes
//  font[c * 5 + i] directly - it does NOT have Adafruit_GFX's "classic charset"
//  quirk that silently shifts every code above 175 by one, so 0xF8 really is
//  0xF8 here. And the advance stays 6 * textsize whatever the character is, so
//  Layout_TextW() and every column constant derived from it remain exact. The
//  unit simply gets narrower: "18 degC" was seven characters, "18 \xF8C" is
//  five.
//
//  Write it as the ESCAPE, never as a literal "°" in the source: these files
//  are UTF-8, where that character is two bytes (0xC2 0xB0) and would print as
//  two pieces of nonsense.
//
//  And note the SPLIT literal below. "\xF8C" does not mean what it looks like:
//  C is a hex digit, so the compiler reads the escape as \xF8C, one character
//  value of 3980, which does not fit in a char. Two adjacent literals are
//  concatenated after the escape has been resolved, which is the whole trick.
//
//  Set to 0 to go back to spelling it out, for a display or a font where the
//  glyph really is missing.
// ---------------------------------------------------------------------------
#define OUTSIDE_DEG_SYMBOL 1
#if OUTSIDE_DEG_SYMBOL
  #define OUTSIDE_DEG_TEXT "\xF8" "C"
#else
  #define OUTSIDE_DEG_TEXT "degC"
#endif

// ---------------------------------------------------------------------------
//  Flight route lookup (adsb.lol) - free, no key, no registration.
//  Asked only when an aircraft's detail is opened, one aircraft at a time.
//  The full path is BASE/{callsign}/{lat}/{lon} - the position goes with the
//  request so the server can say whether the route fits where the aircraft
//  actually is (see Route.h).
// ---------------------------------------------------------------------------
#define ROUTE_API_BASE "https://api.adsb.lol/api/0/route"
#define ROUTE_CACHE_N  8     // remembered answers (keyed on the callsign)

// ---------------------------------------------------------------------------
//  Map orientation
//  The user picks which compass bearing sits at the TOP of the aircraft radar,
//  i.e. the direction they are looking. The step must divide 90 evenly,
//  otherwise the exact cardinal directions (east / west) become unreachable.
// ---------------------------------------------------------------------------
#define MAP_ROT_STEP_DEG 45    // degrees per button press (45 -> 8 positions)

// ---------------------------------------------------------------------------
//  Touch
// ---------------------------------------------------------------------------
// Both controllers drop the odd sample in the middle of a drag, and a failed
// I2C read is thrown away for the same reason (see Touch_*.cpp). Ending the
// gesture on the first empty sample would turn one swipe into several bogus
// taps - so require this much continuous silence before accepting that the
// finger is really up. Real gestures last 40 ms and up, so 60 ms costs nothing.
#define TOUCH_RELEASE_MS 60

// 1 = only read the controller when it says it has something to report.
//
// Both controllers signal a touch event by pulling INT low; reading the
// registers at any other moment returns stale or garbage data (on the GT911 it
// is the previous report, which it will not refresh until the ready flag is
// cleared), and either chip puts itself into standby when nothing is
// happening, where it may not answer at all. We
// used to poll it every few milliseconds regardless - which is where the all
// 0xFF samples came from. The pin is watched by an interrupt (a level check
// would miss the pulse while a frame is being drawn), and there is still a slow
// fallback poll so a lost edge cannot leave the touch dead.
//
// Set to 0 to go back to unconditional polling if the touch ever feels
// unresponsive on some board revision.
#define TOUCH_USE_INT 1

// Fallback poll while idle - only a safety net for a missed interrupt.
#define TOUCH_IDLE_POLL_MS 250

// How often to read the I/O expander back and repair it if it does not match
// what we wrote (TCA9554_Verify). Cheap - one I2C register read.
#define EXPANDER_CHECK_MS 5000UL

// ---------------------------------------------------------------------------
//  Display watchdog
//
//  Reports of a screen that goes black after anywhere from ten minutes to two
//  hours, with the backlight still on and the board otherwise alive. The task
//  watchdog cannot catch that: loop() keeps running, so it keeps being fed.
//  This one watches the panel instead - the VSYNC interrupt counts frames, and
//  if that count stops moving, the display is gone.
// ---------------------------------------------------------------------------
#define DISPLAY_WD 1              // 0 = do not watch the panel at all

// No frame for this long = the panel is dead. One frame is ~34 ms, so anything
// above a second is already far outside normal.
#define DISPLAY_WD_DEAD_MS 3000UL

// First a repair is attempted (put the expander back). If the panel is still
// not scanning this long after that, reboot - the users are power-cycling the
// board by hand anyway, this just does it for them.
#define DISPLAY_WD_REBOOT_MS 8000UL

// ---------------------------------------------------------------------------
//  Network
// ---------------------------------------------------------------------------
// A TLS handshake needs roughly 45 kB of internal RAM. Starting one with less
// than this free fails deep inside mbedTLS and surfaces as a bare "HTTP -1",
// so skip the poll instead and try again later.
#define NET_MIN_HEAP 60000

// WiFiClientSecure defaults the mbedTLS handshake to 120 s, six times
// WDT_TIMEOUT_S. setConnectTimeout() does NOT cover it - that only bounds the
// TCP connect (and without it the connect itself defaults to 30 s, also over
// the watchdog). The handshake loop in ssl_client.cpp has its own limit and
// runs inside http.GET(), where nothing feeds the watchdog. Seconds, not ms.
#define NET_TLS_HANDSHAKE_S 8

// Wall-clock ceiling for receiving one body. HTTPClient::writeToStreamDataBlock
// spins on delay(1) with no timeout of its own, so a server that stops sending
// while holding the socket open would hang until the hardware watchdog fires.
// The sink enforces this instead and the previous data stays on screen.
#define NET_BODY_BUDGET_MS 15000UL

// Strop pro textove odpovedi ctene pres Net_GetString(). Nejvetsi z nich (index
// RainVieweru) ma nizke desitky kB. Strop je tu proto, aby rostouci odpoved dala
// o sobe vedet radkem v logu, misto aby tise ujidala pamet.
#define NET_MAX_TEXT (128 * 1024)

// Hlavicka User-Agent pro VSECHNY odchozi dotazy. Neni to jen zdvorilost:
// adsb.lol odpovi 403 s telem "User-Agent too generic; include valid contact
// info", kdyz v ni zadny kontakt nevidi - a presne to delala vychozi
// "ESP32HTTPClient", ktera odchazela, dokud se hlavicka omylem nastavovala
// pres addHeader() (to ji tise zahazuje, viz ADSB.cpp). Odkaz na web projektu
// jako kontakt staci. Kdyz projekt forknete, dejte sem SVUJ - jinak pujdou
// pripadne stiznosti na cizi adresu.
#define HTTP_USER_AGENT "MeteoPlaneRadar/" FW_VERSION " (+https://chiptron.cz)"

// ---------------------------------------------------------------------------
//  Aircraft detail
// ---------------------------------------------------------------------------
// adsb.fi occasionally drops an aircraft from a single poll and sends it again
// in the next one. Closing the detail panel on the first miss looks like the
// panel closes by itself, so tolerate this many consecutive misses first.
#define DETAIL_GRACE_POLLS 2

// ---------------------------------------------------------------------------
//  Diagnostics
//
//  The serial log comes out at 115200 Bd over the connector marked "USB" -
//  that is the ESP32-S3's native USB. Nothing shows up on the other USB-C
//  connector on the board.
// ---------------------------------------------------------------------------
// 1 = log touch gestures and every aircraft-selection change (with the reason
// why the detail closed) to the serial console at 115200 Bd.
#define TOUCH_DEBUG 0

// 1 = measure how long one full-screen flush takes and print min/last/max once
// per second. Use this to diagnose a flickering band: one frame lasts ~34 ms,
// so if the flush takes anywhere near that, the copy and the panel's scan-out
// run at the same speed and keep crossing each other. Set to 0 when done.
#define FLUSH_DEBUG 0

// ---------------------------------------------------------------------------
//  Watchdog
// ---------------------------------------------------------------------------
#define WDT_TIMEOUT_S 20       // reboot after this many seconds of being stuck

// =============================================================================
//  0.6.0 additions
// =============================================================================

// ---------------------------------------------------------------------------
//  Screens
//
//  Seven of them now, and the six data screens can each be switched off in the
//  web UI. Settings is always reachable - without it there would be no way back
//  to the web UI if someone turned everything else off.
//
//  0.7.0 note: the two energy screens were APPENDED rather than inserted, and
//  that is deliberate. Settings_ScreenEnabled() reads a bit per index out of
//  NVS, so renumbering the existing screens would have silently switched a
//  different set of them off on every device already in the field. Appending
//  leaves bits 0..3 meaning exactly what they always did; the new bits 4 and 5
//  read back as 0 on an existing device, so the energy screens arrive switched
//  off and are discovered in the web UI rather than sprung on the user.
//
//  The one visible side effect: a device that happened to be sitting on the
//  Settings screen when it was updated has "4" stored as its last screen, which
//  now means Prices. That screen is off by default, so the boot check in the
//  .ino notices and falls back to the first enabled one. It corrects itself.
//
//  0.9.0 did the same again for the cards screen: it took 6, Settings moved to
//  7, and bit 6 reads back as 0, so cards arrive switched off.
// ---------------------------------------------------------------------------
#define SCREEN_CLOCK_I    0
#define SCREEN_PLANES_I   1
#define SCREEN_METEO_I    2
#define SCREEN_FORECAST_I 3
#define SCREEN_PRICE_I    4
#define SCREEN_MIX_I      5
#define SCREEN_CARDS_I    6    // 0.9.0 - user-installed JSON cards (Cards.h)
#define SCREEN_SETTINGS_I 7
#define SCREEN_N          8

// Automatic screen cycling: 0 = off, otherwise SECONDS between switches (it was
// minutes up to 0.6.0 - see Settings.h).
//
// A deliberate gesture - a swipe, a long press, or a command from the browser -
// pauses the cycling so you are not fought while reading something. The pause is
// derived from the interval rather than fixed: back when the interval was in
// minutes a flat ten minutes was proportionate, but for a 15-second cycle it
// meant the device stopped cycling the first time anyone touched it.
#define AUTO_ROTATE_PAUSE_FACTOR 3        // pause = 3x the cycling interval
#define AUTO_ROTATE_PAUSE_MIN_MS  30000UL // ...but at least half a minute
#define AUTO_ROTATE_PAUSE_MAX_MS 600000UL // ...and at most ten minutes

// ---------------------------------------------------------------------------
//  Weather radar source
// ---------------------------------------------------------------------------
//
// 0.8.0: the masked CHMU composite was APPENDED as 2 rather than slotted in
// next to plain CHMU, for the same reason the energy screens were appended:
// the number is what is stored in NVS, and renumbering would silently switch an
// existing device from RainViewer to something else on update.
//
//   CHMU         maxz/png/        - the plain MAX_Z composite. Every echo the
//                                   radars see, at full colour.
//   CHMU masked  maxz/png_masked/ - the same MAX_Z picture, but CHMU checks it
//                                   against PseudoCAPPI 2 km: only where the
//                                   rain actually reaches the ground (>= 7 dBZ
//                                   at 2 km) is it drawn at full colour; the
//                                   rest - rain evaporating on the way down,
//                                   anvils of distant storms - comes out paler.
//                                   Same file names, same geometry, so it goes
//                                   through exactly the same code.
#define RADAR_SRC_CHMU        0
#define RADAR_SRC_RAINVIEWER  1
#define RADAR_SRC_CHMU_MASKED 2
#define RADAR_SRC_MAX         2

// RainViewer: free, no key, non-commercial. The JSON lists the available
// frames, the tiles come from the tile cache in standard Web Mercator z/x/y.
#define RV_INDEX_URL "https://api.rainviewer.com/public/weather-maps.json"
#define RV_TILE_SIZE 256        // px per tile as served

// Colour scheme. RainViewer publishes exactly ONE scheme for radar data -
// 2, "Universal Blue" - and the colour table on their site is the palette the
// legend on the weather screen is built from. Asking for any other number is
// not a different look, it is an invalid request.
#define RV_COLOR     2
#define RV_SMOOTH    1
#define RV_SNOW      1
// Their tile service stops at zoom 7 ("Maximum zoom level is 7" in the API
// docs). That is about 790 m per pixel at our latitude, so a 25 km view cannot
// be had by zooming in - the tiles simply do not exist and every request comes
// back 404. Closer ranges are therefore reached by fetching zoom 7 and scaling
// the tile up by a power of two, which also means far fewer tiles per frame.
#define RV_MAX_ZOOM  7
#define RV_MAX_SCALE 8          // 1, 2, 4 or 8 display pixels per tile pixel

// A tile that fails is retried before the frame gives up on it. Without this a
// single dropped connection leaves a permanent black square in the picture.
//
// The wait between attempts matters as much as the count: retrying immediately
// just fails again for the same reason (a busy server, or a moment with too
// little heap for a handshake). A few hundred milliseconds is enough for both
// to pass.
#define RV_TILE_RETRY    3
#define RV_TILE_RETRY_MS 400
#define RV_ANIM_MAX  6          // frames kept, to match the CHMU animation
// A tile PNG is usually a few kB, but a tile full of heavy precipitation is far
// bigger - and a response that does not fit the buffer is refused outright,
// which showed up as squares that went missing exactly where it was raining
// hardest. 64 kB of PSRAM costs nothing and removes the ceiling in practice.
#define RV_MAX_PNG   65536

// ---------------------------------------------------------------------------
//  Forecast, sunrise/sunset and air quality (all Open-Meteo, free, no key)
// ---------------------------------------------------------------------------
#define FORECAST_URL     "https://api.open-meteo.com/v1/forecast"
#define AIRQUALITY_URL   "https://air-quality-api.open-meteo.com/v1/air-quality"
#define GEOCODE_URL      "https://geocoding-api.open-meteo.com/v1/search"

// Hours kept from the hourly block. forecast_hours starts at the current hour,
// so the index IS the offset in hours.
//
// The screen shows "now" and then two WINDOWS of FORECAST_WIN_H hours each,
// labelled by the clock ("12-15h"). A window says something a point value
// cannot: how much rain falls across the afternoon, how hard it gets at its
// worst. Two windows of three hours need indices 1..6, hence seven.
#define FORECAST_HOURS   7
#define FORECAST_WIN_H   3      // hours per window

// Days kept. Index 0 is TODAY (0.7.0 - it used to be skipped), so this is
// today + six. How many of them actually get a row is worked out at draw time
// from the space left inside the circle; the rest are fetched and ignored,
// which costs one URL parameter and no extra request.
#define FORECAST_DAYS    7
#define FORECAST_PERIOD_MS 1800000UL   // 30 min
#define FORECAST_RETRY_MS   120000UL   // 2 min after a failure

#define AQ_PERIOD_MS      1800000UL
#define AQ_RETRY_MS        120000UL

// ---------------------------------------------------------------------------
//  Energy: spot electricity price and the Czech generation mix
//
//  Both free, both without a key and without registration - the same bar every
//  other source in this project has to clear.
//
//  PRICE: spotovaelektrina.cz republishes the OTE day-ahead result already
//  converted to CZK, today and tomorrow in ONE answer. Going to OTE directly
//  would mean parsing a chart-configuration object and doing the EUR conversion
//  here, for the same numbers.
//
//  QUARTER HOURS, not hours. This is the -qh endpoint and the distinction is
//  not academic: OTE trades in fifteen-minute blocks, and up to 0.7.6 the
//  firmware used the plain get-prices-json, which the API's own documentation
//  marks DEPRECATED because it returns hourly AVERAGES. On a flat night the two
//  agree to a few haleru. On an afternoon ramp they do not - at 16:05 on
//  15.9.2026 the quarter hour cost 2721 CZK/MWh while the hourly average for
//  16:00-17:00 was 4023, and the screen confidently showed the average. Half a
//  crown per kilowatt hour is the difference between "run the dishwasher now"
//  and "wait fifteen minutes".
//
//    {"hoursToday":[{"hour":0,"minute":0,"priceEur":217.72,"priceCZK":5291,
//                    "level":"medium","levelNum":16,"levelNum96":67}, ...],
//     "hoursTomorrow":[ ... ]}
//
//  96 entries per day. priceCZK is per MEGAWATT hour. Everything below the
//  fetcher works in CZK/MWh for that reason and only the screen divides by a
//  thousand - which is also the unit most Czech price sites quote, so a number
//  from the screen and a number from a website can be compared directly once
//  you know one is per kWh and the other per MWh.
//
//  hoursTomorrow is empty until the day-ahead auction clears, which is around
//  14:00 local - so "no data for tomorrow" is the normal state for most of the
//  day and must not look like a failure.
//
//  MIX: the Fraunhofer ISE energy-charts API, which serves ENTSO-E data for the
//  Czech bidding zone without the token ENTSO-E itself requires.
//
//    {"unix_seconds":[...], "production_types":[{"name":"Nuclear","data":[...]}]}
//
//  The window matters. Asked for a whole day it answers with ~150 samples for
//  each of twenty types - well over a hundred kilobytes, which is more than the
//  internal heap can hold as a String. Asked for the last few hours it answers
//  with a couple of kilobytes, and since we only ever want the most recent
//  sample, that is all that is worth asking for.
// ---------------------------------------------------------------------------
#define PRICE_URL "https://spotovaelektrina.cz/api/v1/price/get-prices-json-qh"
#define MIX_URL   "https://api.energy-charts.info/public_power"

// Default country code for the mix endpoint. Only the default: the actual one
// is a setting (Settings_MixCountry), because energy-charts serves 38 European
// countries plus "eu" for the continent as a whole, and a device in Slovakia or
// Austria should be able to watch its own grid.
#define MIX_COUNTRY_DEFAULT "cz"

// ---------------------------------------------------------------------------
//  Where these two screens mean anything
//
//  Neither source is global, and a screen that shows a foreign country's data
//  without saying so is worse than no screen at all.
//
//  PRICES are the Czech day-ahead market in CZK. There is no sensible way to
//  show them to someone in Texas, so outside this box the screen says so and
//  the fetcher stays quiet - no point spending someone else's free API quota on
//  numbers nobody can use.
//
//  THE MIX works anywhere energy-charts has a country, which is Europe. Outside
//  that box the same treatment applies.
//
//  Boxes rather than borders, deliberately. A proper point-in-polygon test for
//  Czechia would be a kilobyte of coastline to decide something the user can
//  settle with one tick in the web UI, and the box errs towards showing the
//  screen near the border, which is the friendlier mistake: someone in Zittau
//  or Hainburg may well want Czech prices.
// ---------------------------------------------------------------------------
#define PRICE_AREA_LAT_MIN  48.3f
#define PRICE_AREA_LAT_MAX  51.3f
#define PRICE_AREA_LON_MIN  11.8f
#define PRICE_AREA_LON_MAX  19.2f

#define MIX_AREA_LAT_MIN    34.0f
#define MIX_AREA_LAT_MAX    72.0f
#define MIX_AREA_LON_MIN   -25.0f
#define MIX_AREA_LON_MAX    45.0f

// How far back to ask for. The published data USUALLY runs one to two hours
// behind real time - but not always: on 25.10.2026 the newest Czech sample was
// eleven hours old, a 4-hour window came back as "404 no content available"
// and the screen sat on "loading" all day. So the window is widened step by
// step until something comes back. The narrow one goes first because it is a
// couple of kilobytes; 36 hours of all twenty production types is still only
// a few tens of kB.
#define MIX_WINDOW_H      4
#define MIX_WINDOWS_H     { MIX_WINDOW_H, 12, 36 }

// The day-ahead price is fixed for the whole day, so this is really only about
// picking up tomorrow's prices once they appear after ~14:00.
#define PRICE_PERIOD_MS  1800000UL   // 30 min
#define PRICE_RETRY_MS    120000UL   // 2 min after a failure

// The mix is quarter-hourly at source. Polling faster than that only repeats
// the same sample.
#define MIX_PERIOD_MS     900000UL   // 15 min
#define MIX_RETRY_MS      120000UL

// Ceiling for the price ring's colour scale, in CZK/MWh. Below this the colours
// are stretched over the day's own range, which is what makes a flat day
// readable at all; above it the scale is pinned here so that a genuinely
// expensive day looks expensive rather than merely "red at the top".
#define PRICE_SCALE_MAX 6000.0f

// A negative price is not an error - it happens on windy Sundays and is exactly
// when you want to hear about it. Drawn in its own colour rather than as the
// bottom of the normal scale.
#define PRICE_NEGATIVE_BELOW 0.0f

// How far the sum of the generation slices may sit from load plus exports
// before the status page says the mix looks incomplete. Losses and pumping put
// a few per cent between them legitimately; a whole missing production type
// puts far more. See the check at the end of fetchMix().
#define MIX_SANITY_TOL 0.12f

// ---------------------------------------------------------------------------
//  Clock screen
//
//  The time still comes from the "Date" header of the responses we make anyway
//  (see Outside.h). With only the clock screen enabled nothing else would ever
//  be fetched, so a lightweight HEAD request keeps it seeded.
// ---------------------------------------------------------------------------
#define CLOCK_RESEED_MS   900000UL     // 15 min
#define CLOCK_RESEED_URL  "https://api.open-meteo.com/v1/forecast?latitude=0&longitude=0&current=temperature_2m"

// Seconds ring styles.
#define SEC_STYLE_OFF   0
#define SEC_STYLE_DOTS  1
#define SEC_STYLE_LINE  2
#define SEC_STYLE_COMET 3

// Clock typeface (0.8.0). The digits used to be the built-in 5x7 font blown up
// eight times, which at 64 px is a staircase. The new ones are proper outlines
// rendered with 16 levels of coverage (see AAFont.h / tools/make_fonts.py).
// The pixel font stays as an option for anyone who liked it.
#define CLOCK_FONT_PIXEL   0
#define CLOCK_FONT_INTER   1     // Inter Display Medium - the default
#define CLOCK_FONT_POPPINS 2     // Poppins Medium - rounder
#define CLOCK_FONT_THIN    3     // Inter Display Light - thin
#define CLOCK_FONT_MAX     3
#define CLOCK_FONT_DEFAULT CLOCK_FONT_INTER

// ---------------------------------------------------------------------------
//  Night mode
//
//  Sunrise/sunset arrive with the forecast, so the automatic day/night switch
//  costs no extra request. The offsets let you start dimming before the sun is
//  actually down - useful in a room that goes dark early.
// ---------------------------------------------------------------------------
#define NIGHT_OFFSET_MIN_LIMIT 120     // +/- minutes allowed around sun events

// ---------------------------------------------------------------------------
//  Web configuration server
//
//  Runs permanently once we are on the home WiFi (port 80), and serves the
//  captive setup portal while we are still an access point.
// ---------------------------------------------------------------------------
#define WEB_PORT       80
#define WEB_HOSTNAME   "meteoplaneradar"   // -> http://meteoplaneradar.local/
#define WEB_ADMIN_USER "admin"

// ---------------------------------------------------------------------------
//  Aircraft filters and alerts
// ---------------------------------------------------------------------------
// Emergency squawks: 7500 hijack, 7600 radio failure, 7700 general emergency.
#define SQUAWK_HIJACK "7500"
#define SQUAWK_RADIO  "7600"
#define SQUAWK_EMERG  "7700"

// ---------------------------------------------------------------------------
//  Layout self-check
//
//  1 = at boot, walk every screen's fixed bands and report any overlap on the
//  serial line. Costs nothing at runtime and catches a mis-typed constant
//  before it turns into two labels sitting on top of each other.
// ---------------------------------------------------------------------------
#define LAYOUT_DEBUG 0

// =============================================================================
//  0.8.0 additions
// =============================================================================

// ---------------------------------------------------------------------------
//  Altitude colours on the aircraft radar
//
//  Three boundaries split the sky into four bands. Both the boundaries and the
//  four colours are settings now; these are only the defaults, and they are
//  exactly what the radar always drew, so nothing changes until someone goes
//  looking for the option.
// ---------------------------------------------------------------------------
#define ALT_BAND1_M_DEFAULT  2000     // below: approach / departure, light aircraft
#define ALT_BAND2_M_DEFAULT  6000
#define ALT_BAND3_M_DEFAULT 10000     // above: long-haul cruise
#define ALT_COL0_DEFAULT 0xF800       // red
#define ALT_COL1_DEFAULT 0xFC00       // orange
#define ALT_COL2_DEFAULT 0xFFE0       // yellow
#define ALT_COL3_DEFAULT 0x001F       // blue

// ---------------------------------------------------------------------------
//  Storm watch - the weather radar joins the screen cycling only while there
//  is something worth seeing.
//
//  Every STORM_PERIOD_MS the newest CHMU frame is checked: is there an echo of
//  at least the configured dBZ within the configured distance? CHMU publishes a
//  frame every five minutes, so checking more often only re-reads the same one.
//
//  CHMU, not RainViewer, whichever the screen uses: it is one small image with
//  a known colour scale, where RainViewer would be a dozen tiles. Outside the
//  CHMU coverage the watch has nothing to look at and says so on the status
//  page instead of pretending the sky is clear.
// ---------------------------------------------------------------------------
#define STORM_DBZ_DEFAULT   20
#define STORM_KM_DEFAULT    50
#define STORM_PERIOD_MS     300000UL   // 5 min, the CHMU cadence
#define STORM_RETRY_MS       90000UL   // sooner after a failed fetch
// A single pixel is one square kilometre and can be a radar artefact. This many
// over the threshold make it real.
#define STORM_MIN_PIXELS    3
// How many consecutive clear checks it takes to drop the radar out of the
// cycling again. One clear frame between two stormy ones should not make the
// screen flicker in and out.
#define STORM_CLEAR_CHECKS  2

// ---------------------------------------------------------------------------
//  Aircraft nearby - the aircraft radar jumps in for a few seconds when
//  something flies close (or past the window), then the cycling carries on.
// ---------------------------------------------------------------------------
#define PLANE_ALERT_KM_DEFAULT   5
#define PLANE_ALERT_SEC_DEFAULT  15
// How often to ask adsb.fi while the aircraft screen is NOT showing. The area
// asked for is small (the alert radius or the window distance), so the answer
// is a few hundred bytes. An airliner at cruise covers 5 km in about twenty
// seconds, so polling slower than this would miss most of them.
#define PLANE_WATCH_PERIOD_MS    8000UL
// One aircraft is announced once. It is remembered for this long, so a plane
// circling overhead does not interrupt the cycling every eight seconds.
#define PLANE_ALERT_FORGET_MS    900000UL   // 15 min
#define PLANE_ALERT_MEMORY       12

// The window. Bearing of its centre, how wide the view is, and how far you can
// realistically make out an aircraft through it.
#define WINDOW_AZ_DEFAULT        180
#define WINDOW_WIDTH_DEFAULT      90
#define WINDOW_KM_DEFAULT         15

// ---------------------------------------------------------------------------
//  WiFi - up to three remembered networks
// ---------------------------------------------------------------------------
#define WIFI_SLOTS 3
// While the link is down, the device first retries the network it was on (a
// router reboot is the usual reason) and only after this long looks around for
// any of the other remembered ones.
#define WIFI_SWITCH_AFTER_MS 60000UL
#define WIFI_TRY_MS          12000UL    // per network at boot

// ---------------------------------------------------------------------------
//  Home Assistant
//
//  GET http://meteoplaneradar.local/api/ha returns one flat JSON object with
//  everything the device knows - weather, nearest aircraft, storm, price - for
//  Home Assistant's RESTful integration to poll. Off by default; switched on in
//  the web UI. NOT TESTED against a real Home Assistant - see the README.
// ---------------------------------------------------------------------------
#define HA_DEFAULT_ON false

// ---------------------------------------------------------------------------
//  adsb.fi overloaded or down (0.8.4)
//
//  A failed poll used to be retried at once and then again every 10-30 s,
//  which against an overloaded server is exactly the wrong thing. Now every
//  consecutive failure doubles a shared pause (all callers - the radar, the
//  nearby watch, Home Assistant - go through the same gate), up to
//  ADSB_BACKOFF_MAX_MS. 429 and 5xx are not retried within the same poll, and a
//  Retry-After from the server is honoured. The first good answer resets it.
//
//  Old aircraft are not passed off as live: after ADSB_STALE_MS without fresh
//  data they turn grey and the line under the clock says how old they are;
//  after ADSB_HIDE_MS they are not drawn at all. A stale detail panel no
//  longer holds the screen cycling, and no alert is raised from stale data.
// ---------------------------------------------------------------------------
#define ADSB_BACKOFF_BASE_MS   10000UL     // pause after the first failure
#define ADSB_BACKOFF_MAX_MS   300000UL     // 5 min at most
#define ADSB_RETRY_AFTER_MAX_S   900       // trust Retry-After up to 15 min
#define ADSB_STALE_MS          60000UL     // grey after one minute
#define ADSB_HIDE_MS          600000UL     // gone after ten

// ---------------------------------------------------------------------------
//  JSON cards (0.9.0) - see Cards.h
// ---------------------------------------------------------------------------
#define CARDS_MAX          8         // installed at once
#define CARD_ROWS_MAX      4         // values on one card
#define CARD_FILE_MAX      2048      // bytes of one card's JSON
#define CARD_REFRESH_MIN   5         // minutes; a card may not ask for less
#define CARD_REFRESH_DEF   60
#define CARD_RETRY_MS      120000UL  // after a failed fetch
