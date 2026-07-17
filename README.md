# MeteoPlaneRadar

**Živý radar letadel (ADS-B) a srážkový meteoradar ČHMÚ na kulatém dotykovém
displeji.** Zařízení běží na desce Waveshare ESP32‑S3‑Touch‑LCD‑2.1 a v jednom
přístroji spojuje sledování letadel v okolí a animovanou srážkovou situaci nad
Českou republikou.

> Za vývojem stojí **[chiptron.cz](https://chiptron.cz)** a Claude AI.

---

## Co to je

MeteoPlaneRadar je samostatné WiFi zařízení s kulatým 480×480 displejem, které:

- na **radaru letadel** vykresluje letadla v okolí z volného API **adsb.fi**,
  včetně detailu vybraného letu (výška, rychlost, kurz, stoupání/klesání, typ),
- na **meteoradaru** zobrazuje animovaný srážkový kompozit **ČHMÚ** s obrysem ČR,
  městy a legendou (dBZ / mm/h),
- polohu si zjistí automaticky podle IP (ip‑api.com), nebo ji zadáte ručně.

Poloha, obrys i města se promítají stejnou projekcí jako data, takže mapa vždy
sedí bez ohledu na zvolený rozsah.

## Hardware

| Součást | Popis |
|---|---|
| Deska | **Waveshare ESP32‑S3‑Touch‑LCD‑2.1** |
| MCU | ESP32‑S3R8 (8 MB PSRAM, 16 MB flash) |
| Displej | kulatý **480×480**, řadič **ST7701** (RGB rozhraní) |
| Dotyk | kapacitní **CST820** (I²C) |
| Expandér | **TCA9554** (reset / CS / napájení displeje) |

Konfigurace: **PSRAM = OPI**, **Flash = 16 MB**, **Partition scheme = 16 MB FLASH (3 MB APP / 9.9 MB FATFS)**. Napájení stačí z USB‑C.

## Co to umí

- **Radar letadel** (adsb.fi) — ikony letadel otočené podle kurzu, barva podle
  výšky (do 2 km červená, 2–6 km oranžová, 6–10 km žlutá, nad 10 km modrá),
  callsign u letadla, kroužek u vybraného letu a plovoucí detailní panel.
  Odolné vůči **nekompletnímu / uříznutému JSON** — při chybě stahování zůstanou
  poslední platná data místo zablikání na prázdno.
- **Meteoradar ČHMÚ** — srážkový kompozit s **animací** (až 6 snímků, krok
  5 min, ~2 sn./s, pauza mezi cykly), **indikací času** ke každému snímku
  („nyní / −X min“ + HH:MM), legendou dBZ / mm/h, obrysem ČR a městy. Obraz je
  maskovaný do kruhu displeje.
- **Nastavení** — jas, WiFi (captive portál s QR kódem), poloha.
- **Bez blikání pixelů** — celý snímek se kreslí do jednoho bufferu v PSRAM a
  na panel se posílá jedním přenosem; RGB časování a konfigurace framebufferu
  jsou nastavené tak, aby se sběrnice PSRAM nepřetěžovala (ověřená kombinace).
- **Provoz 24/7** — hardwarový watchdog.

## Ovládání

Ovládá se gesty na dotykovém displeji:

| Gesto | Akce |
|---|---|
| **Swipe** vlevo/vpravo | změna rozsahu (na letadlech i meteoradaru) |
| **Krátký stisk** | výběr letadla / detail (zavření klepnutím mimo) |
| **Dlouhý stisk** | přepnutí obrazovky: Letadla → Meteoradar → Nastavení |
| **Držení BOOT při startu (~3 s)** | tovární reset (WiFi + nastavení) |

Při prvním zapnutí (nebo po resetu) vytvoří zařízení WiFi síť
**`MeteoPlaneRadar`** — připojte se (v portálu je i QR kód) a zadejte údaje své
sítě.

## Závislosti

Arduino IDE, **ESP32 core 3.x**, a knihovny z Library Manageru:

- **GFX Library for Arduino** (moononournation) — kreslení
- **PNGdec** (bitbank2) — dekódování snímků meteoradaru
- **ArduinoJson** (bblanchon) — parsování dat ADS‑B
- **WiFiManager** (tzapu) — konfigurační WiFi portál
- **QRCode** (ricmoo) — QR kód v portálu *(přibaleno v projektu)*

`Preferences`, `Wire`, `HTTPClient` a `esp_lcd` jsou součástí ESP32 core.

## Sestavení

1. V Arduino IDE nainstalujte ESP32 core (3.x) a výše uvedené knihovny.
2. Otevřete `src/MeteoPlaneRadar.ino` (název složky a `.ino` se musí shodovat).
3. Deska: **ESP32‑S3**, PSRAM **OPI**, Flash **16 MB**, partition s dostatkem
   místa pro aplikaci.
4. Nahrajte a připojte se k WiFi síti `MeteoPlaneRadar` pro prvotní nastavení.

Nebo stáhněte přibalený BIN soubor a nahrajte ho do desky na esp32flasher.chiptron.cz

## Zdroje dat a API

Jen pro osobní, nekomerční použití — respektujte prosím podmínky poskytovatelů:

- **Letadla:** adsb.fi — <https://adsb.fi>
  API: `https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/{nm}`
- **Srážky (meteoradar):** Český hydrometeorologický ústav (ČHMÚ) —
  <https://opendata.chmi.cz>
  Kompozit: `https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/`
- **Poloha podle IP:** ip‑api.com — <http://ip-api.com>

## Licence

MIT (viz `src/LICENSE`). Kód smíte volně používat, upravovat i komerčně
nasazovat - musíte si ale zařídit komerční využívání používaných API! Nad rámec licence budeme rádi, když na obrazovce nastavení ponecháte
řádek **chiptron.cz** ve stejné velikosti a barvě jako v originále — je to
prosba, ne podmínka.

## Vývoj

Vyvinul **[chiptron.cz](https://chiptron.cz)**.
