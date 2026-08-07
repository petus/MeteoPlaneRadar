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
//           Ondra OK1CDJ / apps.ok1cdj.com
//  Web:     https://chiptron.cz
//  Board:   Waveshare ESP32-S3-Touch-LCD-2.1 (round 480x480 display, ST7701)
// =============================================================================
#pragma once

// ---------------------------------------------------------------------------
//  Board pins / bus
// ---------------------------------------------------------------------------
#define I2C_SDA   15
#define I2C_SCL   7
#define BOOT_PIN  0        // hold at power-up (~3 s) = factory reset

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
#define METEO_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f }

// ---------------------------------------------------------------------------
//  APRS station (aprs.fi)
//
//  Shows the position of ONE configured station on the map. The callsign and a
//  (free) aprs.fi API key are entered in the WiFi portal and stored in NVS
//  (Settings.*). aprs.fi asks callers not to poll more often than every ~15 s.
// ---------------------------------------------------------------------------
#define APRS_API_BASE  "https://api.aprs.fi/api/get"
#define APRS_RANGES_KM { 25.0f, 50.0f, 100.0f, 200.0f }   // stations sit farther out
#define APRS_PERIOD_MS 30000UL    // poll interval (doubled after a failed fetch)

// ---------------------------------------------------------------------------
//  OTA (firmware update over WiFi)
// ---------------------------------------------------------------------------
#define OTA_IDLE_MS 300000UL   // leave OTA mode after this long with no upload

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
// The CST820 drops the odd sample in the middle of a drag, and a failed I2C
// read is thrown away for the same reason (see Touch_CST820.cpp). Ending the
// gesture on the first empty sample would turn one swipe into several bogus
// taps - so require this much continuous silence before accepting that the
// finger is really up. Real gestures last 40 ms and up, so 60 ms costs nothing.
#define TOUCH_RELEASE_MS 60

// After this many consecutive rejected samples the controller is assumed to be
// wedged (typically after an I2C glitch) and gets re-initialised.
#define TOUCH_REINIT_BAD 40

// 0 = never re-initialise the touch controller at runtime.
//
// Worth knowing what this costs: the reset line runs through the TCA9554, and
// that same chip also holds the display's reset, chip-select and POWER. So the
// recovery reaches for the expander exactly when the I2C bus is known to be
// misbehaving - and a corrupted write there switches the panel off for good,
// with the sketch still running (black screen, no watchdog, needs a power
// cycle). Set this to 0 to rule the whole path out when diagnosing that.
//
// DEFAULT IS 0, deliberately. Users did report the black screen; nobody has
// ever reported a wedged touch controller - that was a theoretical failure the
// recovery was written for. Trading a real fault for a hypothetical one is a
// bad deal. If the wedge ever does show up in a log, reset the chip some other
// way than through the expander that holds the display's power line.
#define TOUCH_RECOVERY 0

// Never re-initialise more often than this. A wedged controller stays wedged,
// so hammering it every few hundred milliseconds only multiplies the risk above.
#define TOUCH_RECOVERY_MIN_MS 60000UL

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

// The WiFi portal blocks the whole sketch and the watchdog is suspended while
// it runs, so this timeout is what keeps it from blocking forever.
#define PORTAL_TIMEOUT_S 180

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
