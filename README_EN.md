# MeteoPlaneRadar

**A clock, live aircraft radar, precipitation radar and weather forecast on a
round touchscreen.** Runs on a single Waveshare ESP32-S3-Touch-LCD-2.1 or
ESP32-S3-Touch-LCD-2.8C board and is configured from a browser.

> Built by **[chiptron.cz](https://chiptron.cz)** with Claude AI.
> Czech version of this document: [README.md](README.md)

---

## Switch it to English

The device ships in Czech. There are two ways to change that, and both stick
across restarts.

### On the device

1. **Long-press the right half of the screen** until you reach **Nastaveni**
   (Settings) — it is the last screen in the cycle.
2. Near the bottom there are three wide buttons. Press the **middle** one,
   labelled **`Jazyk: cestina`**.
3. It immediately becomes **`Language: English`** and the whole interface
   follows.

### In the browser

1. Open `http://meteoplaneradar.local/` (the address is also printed on the
   Settings screen).
2. Top right there is a language selector — choose **English**.

The page and the device both switch immediately; there is nothing to save.

The same selector appears in the setup portal during first-time WiFi
configuration, so you can switch before anything else.

> The built-in display font is 7-bit ASCII. Czech is therefore drawn without
> diacritics; English renders exactly as written.

---

## What it is

A standalone WiFi device with a round 480×480 touchscreen. Put it on a shelf
next to your monitor and a glance tells you what is flying overhead and whether
it is about to rain.

It is not a phone and does not try to be one.

## What it does

| Screen | Shows | Source |
| --- | --- | --- |
| **Clock** | time, date, current weather, seconds ring | Open-Meteo |
| **Aircraft** | aircraft around you, tap for details and route | adsb.fi, adsb.lol |
| **Weather radar** | animated precipitation | CHMI or RainViewer |
| **Forecast** | next 6 hours and 3 days, air quality and pollen | Open-Meteo |
| **Settings** | brightness, map orientation, units, language | — |

Any of the first four can be switched off in the browser; Settings is always
reachable.

### Clock

Date, large time, current weather with temperature and rain, wind speed below.
A **seconds ring** runs around the rim — off, dots, a continuous line, or a
comet with a fading tail. Clock and seconds colours are configurable.

No Home Assistant and no sensor: the weather comes from the same request the
forecast screen makes anyway.

There is **no NTP client**. The time is taken from the `Date` header of the
HTTP responses the device makes regardless.

### Aircraft

Aircraft icons are colour-coded by flight level:

| Altitude | Colour | Typically |
| --- | --- | --- |
| Below 2 km | Red | Approach and departure, helicopters, light aircraft |
| 2 to 6 km | Orange | Climb, descent, regional traffic |
| 6 to 10 km | Yellow | Lower cruise levels |
| 10 km and up | Blue | Long-haul cruise |

An aircraft not reporting its altitude is drawn grey, not red.

Tap one for the detail panel: altitude, speed, track, climb rate, aircraft type,
registration and where it is flying from and to. It updates live while open.

**Emergency squawks** 7500 (hijack), 7600 (radio failure) and 7700 (general
emergency) can be watched for. Such an aircraft gets a double red ring and a red
banner replaces the aircraft count. There are also filters — altitude range,
only aircraft with a callsign — and a **watched callsign or ICAO address** that
is highlighted in green. Filters affect drawing only; they never hide an
emergency or a watched aircraft.

**Map orientation:** the `Nahore` / `Top` row in Settings sets which compass
bearing is at the top of the screen. Set the direction you are looking out of
the window and an aircraft seen to the left of the roof appears to the left on
the display. Eight positions, 45° apart. The weather radar deliberately does not
rotate — a precipitation map is read north-up.

### Weather radar

The device downloads the last six frames at five-minute intervals and loops
them, so you can see where the rain is heading. Each frame carries its time
("now" or "−X min" plus HH:MM), and there is an intensity legend in dBZ and
mm/h.

Ranges: 25, 50, 100, 200 km and the whole Czech Republic.

**Two sources, switchable in the browser:**

- **CHMI** (default) — sharper, but the data ends just beyond the Czech border.
  With a location abroad the screen stays blank.
- **RainViewer** — European and global coverage, also free and keyless. Coarser:
  their tiles stop at zoom 7, roughly 790 metres per pixel, so closer ranges are
  built from what is available. That is genuinely all the detail there is.

The legend switches with the source and says which one you are reading — each
radar has its own palette, so the same yellow means 40 dBZ on one scale and 35
on the other.

### Forecast

The next **6 hours** on top, the next **3 days** below the line. Each row has a
vector weather icon derived from the WMO code, temperature, precipitation and
wind. Days show maximum and minimum. Every value carries its unit.

### Air quality and pollen

Three lines at the bottom of the forecast screen: the **European AQI**, **PM2.5**
in µg/m³, and **pollen** — the strongest of alder, birch and grass, with the
species named. Pollen is a European product and simply does not appear
elsewhere.

---

## Hardware

**Waveshare ESP32-S3-Touch-LCD-2.1** or **ESP32-S3-Touch-LCD-2.8C** — ESP32-S3R8
(8 MB PSRAM, 16 MB flash), round 480×480 IPS display with an ST7701 controller,
TCA9554 I/O expander. Touch is a CST820 on the 2.1 and a GT911 on the 2.8C.
One board and a USB-C cable; nothing to solder or wire.

The two boards are pin-for-pin identical and **one binary runs on both** — at
boot the firmware asks the touch controller who it is and picks the panel init
sequence and the timing from the answer. Which board it settled on is in the
serial log at startup and in the status table in the browser. If it ever gets it
wrong on your board (a dead touch controller, most likely), `BOARD_FORCE` in
`Config.h` pins it down.

## First run

1. Flash the firmware (below) and power the board.
2. Join the open WiFi network **`MeteoPlaneRadar`** — a QR code is shown on the
   display.
3. Open `http://192.168.4.1/`, pick your home network and save.

The access point stays up until you enter a network. Without a connection the
device has nothing to fetch and nothing to show.

Afterwards the settings live at **`http://meteoplaneradar.local/`**, or at the
IP address printed on the Settings screen.

## Configuration in a browser

The web interface runs permanently and is split into six tabs so it works on a
phone: **Control** (switch screens and range remotely, device status),
**Location** (manual or by searching for a town name), **Screens** (which to
show, auto-cycling, radar source), **Appearance** (day and night brightness,
sun-driven night mode, seconds ring, colours), **Aircraft** (filters, squawks,
watched callsign, map orientation, units) and **System** (password, firmware
update, settings backup, restart, factory reset).

Most settings are **saved the moment you change them**. The Save button is only
needed for the location, the set of screens and the radar source, because those
restart the device.

The status page reports IP, signal strength, uptime, free memory, the reason for
the last restart and the outcome of the last fetch from each source — diagnostics
without a serial cable.

### Password

The password is optional and **there is none by default**, which means firmware
update, settings import and factory reset are open to anyone who can reach the
device. Set one in the System tab: leave *current password* empty the first
time and fill in *new password* only. A single space in the new password field
removes the protection. The username on `/update` is `admin`.

The password is **stored in the clear**, because the update page uses HTTP Basic
and the library has to be given the real password. It protects against the
household, not against someone who can read the flash.

**A forgotten password can only be cleared by holding the BOOT button at
startup**, which is a factory reset and also erases WiFi.

## Controls

| Gesture | Action |
| --- | --- |
| **Swipe** left/right | Change the range (aircraft, weather radar) |
| **Short tap** | Select an aircraft; on the clock, toggle day/night |
| **Long press, left half** | Previous screen |
| **Long press, right half** | Next screen |
| **Hold BOOT at startup (~3 s)** | Factory reset |

Screens and range can also be changed from the browser, without touching the
glass.

---

## Flashing

**Over USB** (first flash and rescue): download `*.merged.bin` from Releases,
connect the board to the connector marked **USB**, and flash it at
[esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz) in Chrome or Edge.

**Over WiFi:** download `*.ino.bin` (the one **without** `merged`) and upload it
at `http://meteoplaneradar.local/update`. The display goes dark during the
write — the RGB panel streams its framebuffer from PSRAM and flash writes cut
its data off — and lights back up when finished. A failed update leaves the
previous version running.

Versions older than 0.4 must be flashed over USB once; their flash layout has no
room for an over-the-air update.

**Coming from 0.5.x?** The stored WiFi is not carried over — it used to be held
by WiFiManager, which is no longer used. The device will bring up its own
network once and you enter your WiFi again. Location, brightness and map
orientation are kept.

## For developers

Arduino IDE with **ESP32 core 3.x**. Libraries: **GFX Library for Arduino**,
**PNGdec**, **ArduinoJson v7**; QRCode is bundled. `WebServer`, `DNSServer`,
`ESPmDNS`, `Update` and `Preferences` come with the core. WiFiManager was
dropped in 0.6.0, ElegantOTA in 0.6.2.

Board settings: ESP32S3 Dev Module, **PSRAM: OPI** (without this the display
stays black), Flash 16 MB QIO, **Partition Scheme: Custom** (`partitions.csv`,
two app slots for OTA), USB CDC On Boot: Enabled. Or
`arduino-cli compile --profile default MeteoPlaneRadar`.

```
MeteoPlaneRadar.ino   screen manager, touch, main loop
Config.h              all tunable constants
Settings.*            NVS settings + JSON for the web UI
Lang.*                Czech and English strings
Layout.*              screen bands and collision checking
WebConfig.* WebPage.h web server, API, captive portal, OTA
Net.*                 shared HTTPS fetching
Forecast.*            Open-Meteo: forecast, sun times, air quality
RainViewer.*          tile radar
Screen*.{h,cpp}       individual screens
```

**Why nothing overlaps:** each screen reserves its chrome before the map is
drawn. Anything positioned by data — city labels, aircraft callsigns — must then
claim its rectangle and is dropped if the space is taken. The `LY_*` constants in
`Layout.h` keep the same elements on the same lines across screens.

Serial log at 115200 Bd over the connector marked **USB**. The same information
is on the web status page.

---

## Data sources and attribution

Personal, non-commercial use only unless you arrange otherwise. **Several of
these require credit** — see [LICENSE.txt](LICENSE.txt) for the details.

| Data | Source | Note |
| --- | --- | --- |
| Aircraft, registration, type | [adsb.fi](https://adsb.fi) | Free, no key, personal use |
| Route | [adsb.lol](https://adsb.lol) | Free, no key; route data by [vradarserver/standing-data](https://github.com/vradarserver/standing-data) |
| Precipitation (CZ) | [CHMI](https://opendata.chmi.cz) | Attribution required |
| Precipitation (world) | [RainViewer](https://www.rainviewer.com) | Attribution required |
| Weather, forecast, sun, air quality, geocoding | [Open-Meteo](https://open-meteo.com) | CC BY 4.0, attribution required, free tier non-commercial |
| Location by IP | [ip-api.com](http://ip-api.com) | Free tier non-commercial |
| Map | Natural Earth (public domain), [GeoNames](https://www.geonames.org) (CC BY 4.0) | Attribution required |

## Licence

**MIT** — see [LICENSE.txt](LICENSE.txt). It covers the source code and the
binaries built from it. Since 0.6.2 the project has no copyleft dependency.

**MIT does not cover the data.** **GeoNames**, **CHMI**, **RainViewer** and
**Open-Meteo** all require visible attribution, and the free Open-Meteo and
adsb.fi APIs are for non-commercial use only. LICENSE.txt has the details —
**read it in full before any commercial deployment.**

Beyond what the licence requires: if you build on this, I would be glad if you
kept the **chiptron.cz** credit on the settings screen. A request, not a
condition.

## Contributors

- **[Pájeníčko.cz](https://pajenicko.cz)**
  - **Support for the Waveshare ESP32-S3-Touch-LCD-2.8C.** The board is
    identified at boot from its touch controller, so one binary runs on both
    the 2.1 and the 2.8C and nobody has to pick which file to flash.

## Built on

- [petus/MeteoPlaneRadar](https://github.com/petus/MeteoPlaneRadar) — this project
- [ok1cdj/MeteoPlaneRadar](https://github.com/ok1cdj/MeteoPlaneRadar) — Ondra OK1CDJ's fork; the source of the RainViewer, forecast screen, screen toggles and auto-cycling ideas. His version also has an APRS screen and PlatformIO support
- [CooLajz/waveshare-hodiny](https://github.com/CooLajz/waveshare-hodiny) — a Home Assistant dashboard on the same board; inspiration for the clock, seconds ring, night mode and web configuration
- [MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar) — the original aircraft radar and the adsb.fi source
- [Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar](https://github.com/Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar) — the port to Waveshare 480×480
- [mylms/ESP-MeteoRadar](https://github.com/mylms/ESP-MeteoRadar) — the CHMI precipitation radar

Change history: [CHANGELOG.md](CHANGELOG.md). Version: `MeteoPlaneRadar/Version.h`.
