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

// Degree symbol. The built-in font is 7-bit ASCII, so a real "°" may come out
// as a random glyph; "degC" is the safe spelling. Flip to 1 only after checking
// on the actual display that the character renders.
#define OUTSIDE_DEG_SYMBOL 0
#if OUTSIDE_DEG_SYMBOL
  #define OUTSIDE_DEG_TEXT "\xB0C"
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
//  Five of them now, and the four data screens can each be switched off in the
//  web UI. Settings is always reachable - without it there would be no way back
//  to the web UI if someone turned everything else off.
// ---------------------------------------------------------------------------
#define SCREEN_CLOCK_I    0
#define SCREEN_PLANES_I   1
#define SCREEN_METEO_I    2
#define SCREEN_FORECAST_I 3
#define SCREEN_SETTINGS_I 4
#define SCREEN_N          5

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
#define RADAR_SRC_CHMU       0
#define RADAR_SRC_RAINVIEWER 1

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

#define FORECAST_HOURS   6      // hourly rows on the forecast screen
#define FORECAST_DAYS    3      // daily rows under them
#define FORECAST_PERIOD_MS 1800000UL   // 30 min
#define FORECAST_RETRY_MS   120000UL   // 2 min after a failure

#define AQ_PERIOD_MS      1800000UL
#define AQ_RETRY_MS        120000UL

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
