# MeteoPlaneRadar

[![build](https://github.com/ok1cdj/MeteoPlaneRadar/actions/workflows/build.yml/badge.svg)](https://github.com/ok1cdj/MeteoPlaneRadar/actions/workflows/build.yml)

**Živý radar letadel (ADS-B), srážkový meteoradar ČHMÚ a poloha APRS stanice na
kulatém dotykovém displeji.** Zařízení běží na desce Waveshare
ESP32‑S3‑Touch‑LCD‑2.1 a v jednom přístroji spojuje sledování letadel v okolí,
animovanou srážkovou situaci nad Českou republikou a polohu vybrané APRS stanice.

> Za vývojem stojí **[chiptron.cz](https://chiptron.cz)** a Claude AI.

**Článek najdete na** https://chiptron.cz/meteoradar-a-radar-letadel-na-jednom-kulatem-displeji/

---

## Co to je

MeteoPlaneRadar je samostatné WiFi zařízení s kulatým 480×480 displejem, které:

- na **radaru letadel** vykresluje letadla v okolí z volného API **adsb.fi**,
  včetně detailu vybraného letu (výška, rychlost, kurz, stoupání/klesání, typ),
- na **meteoradaru** zobrazuje animovaný srážkový kompozit **ČHMÚ** s obrysem ČR,
  městy a legendou (dBZ / mm/h),
- na **APRS obrazovce** ukáže polohu jedné nastavené stanice z **aprs.fi** na
  mapě vycentrované na stanici, s barevnou indikací stáří poslední pozice,
- polohu si zjistí automaticky podle IP (ip‑api.com), nebo ji zadáte ručně.

Poloha, obrys i města se promítají stejnou projekcí jako data, takže mapa vždy
sedí bez ohledu na zvolený rozsah.

**Programovat nemusíte.** Hotový firmware nahrajete z prohlížeče — viz
[Jak nahrát firmware](#jak-nahrát-firmware).

## Hardware

| Součást | Popis |
|---|---|
| Deska | **Waveshare ESP32‑S3‑Touch‑LCD‑2.1** |
| MCU | ESP32‑S3R8 (8 MB PSRAM, 16 MB flash) |
| Displej | kulatý **480×480**, řadič **ST7701** (RGB rozhraní) |
| Dotyk | kapacitní **CST820** (I²C) |
| Expandér | **TCA9554** (reset / CS / napájení displeje) |

Stačí tahle jedna deska a USB‑C kabel. Nic se nepájí ani nedrátuje.

## Co to umí

- **Radar letadel** (adsb.fi) — ikony letadel otočené podle kurzu, barva podle
  výšky (do 2 km červená, 2–6 km oranžová, 6–10 km žlutá, nad 10 km modrá),
  callsign u letadla, kroužek u vybraného letu a plovoucí detailní panel.
  Odolné vůči **nekompletnímu / uříznutému JSON** — při chybě stahování zůstanou
  poslední platná data místo zablikání na prázdno.
  Interval stahování se řídí rozsahem: 5 s (10/25 km), 10 s (50 km), 15 s
  (100 km). Při chybě se interval zdvojnásobí.
- **Meteoradar ČHMÚ** — srážkový kompozit s **animací** (až 6 snímků, krok
  5 min, ~2 sn./s, pauza mezi cykly), **indikací času** ke každému snímku
  („nyní / −X min“ + HH:MM), legendou dBZ / mm/h, obrysem ČR a městy. Obraz je
  maskovaný do kruhu displeje.
- **APRS stanice** (aprs.fi) — poloha jedné zvolené stanice na mapě, která je
  **vycentrovaná na stanici**; rozsah (poloměr kolem ní) měníte přejetím prstem
  jako u radaru. Dole ukazuje **stáří poslední pozice** barevně (zeleně ≤ 15 min,
  žlutě ≤ 1 h, červeně starší; čas se synchronizuje přes NTP) a u pohyblivých
  stanic rychlost a kurz. Volací znak a bezplatný **aprs.fi API klíč** se zadají
  ve WiFi portálu.
- **Nastavení** — jas, WiFi (captive portál s QR kódem), poloha, orientace
  mapy, zobrazení aktuální verze firmwaru a tlačítko pro bezdrátovou
  aktualizaci.
- **Orientace podle výhledu** — nastavíte, **který světový směr je nahoře na
  displeji** (`S`, `SV`, `V`, …), a letadla na displeji jsou ve stejném směru
  jako ta za oknem. Osm poloh po 45°. Podrobnosti níže v [Ovládání](#ovládání).
- **Aktualizace přes WiFi (OTA)** — nový firmware nahrajete z prohlížeče bez
  USB kabelu.
- **Pamatuje si stav** — poslední zvolený rozsah (zvlášť pro letadla, meteoradar
  i APRS) i naposledy zobrazenou obrazovku; po restartu naskočí tam, kde jste
  skončili.
- **Bez blikání pixelů** — celý snímek se kreslí do jednoho bufferu v PSRAM a
  na panel se posílá jedním přenosem synchronizovaným s VSYNC.
- **Provoz 24/7** — hardwarový watchdog, zotavení dotykového řadiče po zámrzu
  I2C a kontrola volné paměti před každým stahováním.

## Ovládání

Ovládá se gesty na dotykovém displeji:

| Gesto | Akce |
|---|---|
| **Přejetí prstem** vlevo/vpravo | změna rozsahu (na letadlech, meteoradaru i APRS) |
| **Krátké klepnutí** | výběr letadla / detail (zavření klepnutím mimo) |
| **Dlouhý stisk v levé polovině** | předchozí obrazovka |
| **Dlouhý stisk v pravé polovině** | následující obrazovka |
| **Držení BOOT při startu (~3 s)** | tovární reset (WiFi + nastavení) |

Obrazovky jsou čtyři: **Letadla → Meteoradar → APRS → Nastavení** (dokola).

### Orientace mapy (Nastavení → `Nahore`)

Řádek `Nahore` s tlačítky `−` / `+` říká, **který světový směr je nahoře na
displeji**. Nastavte směr, kterým se díváte z okna — letadlo, které vidíte
vlevo nad střechou, bude vlevo nad středem i na displeji.

| `Nahore` | Nahoře je | Sever pak leží |
|---|---|---|
| `S` | sever | nahoře (výchozí) |
| `V` | východ | vlevo |
| `J` | jih | dole |
| `Z` | západ | vpravo |

Mezistupně (`SV`, `JV`, `JZ`, `SZ`) jsou po 45°, `+` jde po směru hodinových
ručiček. Vedle tlačítek je kompasový náhled, po obvodu radaru značky S/V/J/Z.
Nastavení přežije restart. Krok se dá změnit přes `MAP_ROT_STEP_DEG`
v `src/Config.h`, musí ale dělit 90 — jinak přestane být dosažitelný přesný
východ a západ.

**Meteoradar se záměrně neotáčí** — srážková mapa se čte severem nahoru a
orientaci v ní drží obrys ČR.

> Aktualizujete‑li z verze 0.5.1 nebo starší, kde se zadávalo „o kolik mapu
> otočit", orientace se vrátí na sever nahoře. Nastavte si ji prosím znovu.

### APRS stanice (aprs.fi)

Třetí mapová obrazovka ukazuje polohu **jedné nastavené stanice** z
[aprs.fi](https://aprs.fi). Mapa je **vycentrovaná na stanici** a přejetím prstem
měníte poloměr kolem ní (25 / 50 / 100 / 200 km). Barevný údaj dole říká, jak
stará je poslední poloha (**zeleně** ≤ 15 min, **žlutě** ≤ 1 h, **červeně** víc)
— čas se získává přes NTP, takže hned poznáte, jestli jsou data aktuální.

Je-li známá **vaše poloha** a padne do rozsahu, ukáže se jako malý **azurový
bod**, takže vidíte, kde jste vůči stanici. Mapa je tu oproti radaru hustší —
víc měst a o něco větší popisky.

Volací znak a API klíč se zadají ve **WiFi portálu** (Nastavení → WiFi/poloha,
nebo captive portál při prvním připojení) do polí *APRS volací znak stanice* a
*aprs.fi API klíč*. Klíč je **zdarma** — vygenerujete si ho po registraci na
[aprs.fi](https://aprs.fi) v sekci *My Account → API key*. Bez vyplněného znaku
obrazovka jen vypíše výzvu k nastavení.

Při prvním zapnutí (nebo po resetu) vytvoří zařízení WiFi síť
**`MeteoPlaneRadar`** — připojte se (na displeji je i QR kód) a zadejte údaje
své domácí sítě.

---

# Jak nahrát firmware

Jsou dvě možnosti. **Když nevíte, kterou zvolit, použijte Full programming** —
funguje vždy.

| | Full programming | OTA (přes WiFi) |
|---|---|---|
| Čím | prohlížeč + USB‑C kabel | jen prohlížeč, bezdrátově |
| Soubor | `MeteoPlaneRadar_v0.X.Y.ino.merged.bin` | `MeteoPlaneRadar_v0.X.Y.ino.bin` |
| Kdy | první nahrání, přechod z verze nižší než 0.4, záchrana | běžná aktualizace z verze 0.4 a vyšší |

> ### ⚠️ Máte verzi starší než 0.4 (nebo nevíte jakou)?
> **Musíte použít Full programming přes USB.** Verze do 0.3 mají jinak
> rozdělenou paměť a bezdrátová aktualizace nemá kam zapsat — OTA by selhala.
> Stačí to udělat jednou; od verze 0.4 už můžete aktualizovat bezdrátově.
>
> Jakou verzi máte, zjistíte v **Nastavení** — verze je vypsaná pod nadpisem.
> Když tam žádná není, máte verzi starší než 0.4.

## Full programming (USB kabel)

Nahraje celou paměť včetně jejího rozdělení. Funguje vždy, i na úplně nové desce.

1. Stáhněte si z [**Releases**](../../releases) soubor
   **`MeteoPlaneRadar_v0.X.Y.ino.merged.bin`** (ten s `merged`).
2. Připojte desku k počítači USB‑C kabelem — do konektoru označeného **USB**
   (viz poznámka o konektorech níže).
3. Otevřete **[esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz)**
   v prohlížeči **Chrome** nebo **Edge** (Firefox a Safari to neumí).
4. Vyberte **ESP32‑S3**, vyberte stažený soubor a spusťte nahrávání.
5. Po dokončení dejte reset. Zařízení vytvoří WiFi síť `MeteoPlaneRadar` —
   připojte se a zadejte svou domácí WiFi.

## OTA – aktualizace přes WiFi (bez kabelu)

Od verze 0.4 můžete nový firmware nahrát bezdrátově, přímo ze zařízení.

1. Stáhněte si z [**Releases**](../../releases) soubor
   **`MeteoPlaneRadar_v0.X.Y.ino.bin`** — pozor, **ten bez `merged`**.
2. V zařízení jděte do **Nastavení** a klepněte na **Firmware update**.
3. Zařízení vytvoří WiFi síť **`MeteoPlaneRadar`** (bez hesla) a ukáže QR kód.
   Připojte se k ní telefonem nebo notebookem.
4. V prohlížeči otevřete **`http://192.168.4.1/update`**.
5. Vyberte stažený soubor a nahrajte ho. Průběh vidíte v prohlížeči.
6. Zařízení se samo restartuje do nové verze. V Nastavení si ověřte, že se
   verze změnila.

**Co je během aktualizace normální:**

- Displej ukáže „Probiha aktualizace…" a **pak zhasne**. Tak to má být — při
  zápisu do paměti nelze udržet stabilní obraz, proto se podsvícení vypne.
  Po dokončení se samo rozsvítí.
- Telefon hlásí, že síť nemá internet. To nevadí, soubor už máte stažený.
  Případně na chvíli vypněte mobilní data, aby se telefon neodpojoval.

**Kdyby se aktualizace nepovedla:** nic se neděje. Zařízení se vrátí k původní
verzi a vždycky ho můžete zachránit přes Full programming.

Nechcete nakonec nahrávat? Klepnutím na displej režim aktualizace opustíte. Po
5 minutách nečinnosti skončí sám.

---

## Sériový výpis (diagnostika)

Zařízení vypisuje na sériovou linku informace o připojení, stahování dat a
případných chybách. Hodí se, když něco nefunguje a chcete zjistit proč.

> ### ⚠️ Který USB‑C konektor použít
> Deska má **dva USB‑C konektory**. Pro sériový výpis musí být kabel v tom
> označeném **USB** — to je nativní USB vedené přímo do čipu ESP32‑S3.
> Přes druhý konektor se v Sériovém monitoru **nic neobjeví**.

Otevřete Sériový monitor (v Arduino IDE, nebo libovolný terminál) a nastavte
rychlost **115200 Bd**. Po startu uvidíte například:

```
=== MeteoPlaneRadar v0.X.Y ===
Duvod restartu: zapnuti napajeni
Volna pamet: 218432 B
Displej: dvojity framebuffer, kresleni bez kopirovani
CST820 ID: 0xB5
WiFi ok, IP 192.168.1.42
ADSB: 12 aircraft (8421 bytes)
Meteoradar: 6 ramcu
```

První dva řádky jsou v hlášení chyb nejcennější. **Duvod restartu** rozliší
běžné zapnutí od pádu (`PANIC`), zaseknuté smyčky (`WATCHDOG`) nebo slabého
zdroje (`BROWNOUT`). **Volna pamet** je volná interní RAM — když klesne pod
~60 kB, přeskočí se stahování a v logu se objeví `malo volne pameti`.

Podrobnější ladicí výpisy (gesta dotyku, důvody zavření detailu letadla, doba
překreslení) se zapínají v `src/Config.h` přepínači `TOUCH_DEBUG` a
`FLUSH_DEBUG`. Pro běžný provoz je nechte na 0.

## Pro vývojáře: překlad ze zdrojáků

Tahle část je jen pro ty, kdo si chtějí projekt upravit. Pokud jste nahráli
hotový firmware, přeskočte ji.

### Závislosti

Arduino IDE, **ESP32 core 3.x**, a knihovny z Library Manageru:

- **GFX Library for Arduino** (moononournation) — kreslení
- **PNGdec** (bitbank2) — dekódování snímků meteoradaru
- **ArduinoJson** (bblanchon, v7) — parsování dat ADS‑B
- **WiFiManager** (tzapu) — konfigurační WiFi portál
- **ElegantOTA** (ayushsharma82) — aktualizace přes WiFi
- **QRCode** (ricmoo) — QR kód v portálu *(přibaleno v projektu)*

`Preferences`, `Wire`, `HTTPClient`, `WebServer` a `esp_lcd` jsou součástí
ESP32 core.

> ElegantOTA se používá ve **výchozím (synchronním) režimu** — nic se v knihovně
> needituje a `ESPAsyncWebServer` ani `AsyncTCP` nejsou potřeba.

### Nastavení Arduino IDE

| Položka | Hodnota |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | **OPI PSRAM** (bez toho zůstane displej černý) |
| Flash Size | 16MB (128Mb) |
| Flash Mode | QIO 80MHz |
| **Partition Scheme** | **Custom** (použije se přiložený `src/partitions.csv`) |
| USB CDC On Boot | Enabled |

Nahrávat i číst sériový výpis je potřeba přes konektor označený **USB**
(nativní USB čipu ESP32‑S3), ne přes ten druhý.

Partition **Custom** je pro OTA nutná — přiložená tabulka má dvě aplikační
oblasti (2× 6 MB), aby bylo kam nahrát novou verzi. Po překladu zkontrolujte
v logu, že se hlásí `of 6291456 bytes`.

Nastavení jako časová zóna, výchozí poloha, rozsahy nebo limity najdete
pohromadě v **`src/Config.h`**. Verze firmwaru je v **`src/Version.h`**.

### Překlad přes PlatformIO

V kořeni projektu je `platformio.ini`, který nastavuje totéž, co tabulka výše
(ESP32‑S3, OPI PSRAM, 16 MB QIO flash, custom `src/partitions.csv`, USB CDD on
boot) a připíná stejný ESP32 core **3.0.7** i verze knihoven jako
`sketch.yaml`. Používá se komunitní platforma **pioarduino** (oficiální
`espressif32` zatím vozí jen core 2.x).

```bash
pio run              # překlad
pio run -t upload    # nahrání firmwaru (konektor USB)
pio run -t monitor   # sériový monitor (115200 Bd)
```

Knihovny se stáhnou automaticky. `ESPAsyncWebServer`/`AsyncTCP` jsou přes
`lib_ignore` vyřazené — ElegantOTA běží v synchronním režimu a nepotřebuje je.

### Vytvoření souborů pro Releases

- **`MeteoPlaneRadar.ino.bin`** (pro OTA) — *Sketch → Export Compiled Binary*.
- **`MeteoPlaneRadar.ino.merged.bin`** (pro web flasher) — sloučený obraz celé
  paměti; musí být vygenerovaný se **stejnou partition tabulkou**.

Při vydání soubory pojmenujte s verzí, např. `MeteoPlaneRadar_v0.4.ino.bin`.

---

## Zdroje dat a API

Jen pro osobní, nekomerční použití — respektujte prosím podmínky poskytovatelů:

- **Letadla:** adsb.fi — <https://adsb.fi>
  API: `https://opendata.adsb.fi/api/v3/lat/{lat}/lon/{lon}/dist/{nm}`
- **Srážky (meteoradar):** Český hydrometeorologický ústav (ČHMÚ) —
  <https://opendata.chmi.cz>
  Kompozit: `https://opendata.chmi.cz/meteorology/weather/radar/composite/maxz/png/`
- **APRS stanice:** aprs.fi — <https://aprs.fi>
  API: `https://api.aprs.fi/api/get?name={znak}&what=loc&apikey={klíč}&format=json`
  (vyžaduje bezplatný API klíč)
- **Poloha podle IP:** ip‑api.com — <http://ip-api.com>
- **Čas (NTP):** pool.ntp.org, time.google.com — pro stáří APRS pozic

> Meteoradar ČHMÚ pokrývá **Českou republiku a blízké okolí**. Když máte polohu
> nastavenou jinam (třeba do zahraničí), zůstane meteo obrazovka prázdná — data
> pro tu oblast neexistují. Radar letadel funguje kdekoliv.

## Licence

MIT (viz `LICENSE`). Kód smíte volně používat, upravovat i komerčně nasazovat —
musíte si ale zařídit komerční využívání používaných API! Nad rámec licence
budeme rádi, když na obrazovce nastavení ponecháte řádek **chiptron.cz** ve
stejné velikosti a barvě jako v originále — je to prosba, ne podmínka.

## Inspirace

Tento projekt nevznikl z ničeho. Navazuje na tři existující:

[MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar) - původní radar letadel a zdroj adsb.fi

[Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar](https://github.com/Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar) - port na Waveshare 480×480

[mylms/ESP-MeteoRadar](https://github.com/mylms/ESP-MeteoRadar/tree/main) - ČHMÚ srážkový meteoradar

## Vývoj

Vyvinul **[chiptron.cz](https://chiptron.cz)**. Článek o projektu najdete na
[https://chiptron.cz/meteoradar-a-radar-letadel-na-jednom-kulatem-displeji/](https://chiptron.cz/meteoradar-a-radar-letadel-na-jednom-kulatem-displeji/)

Konverzi na **PlatformIO** a **APRS obrazovku** přidal **Ondra OK1CDJ**
([apps.ok1cdj.com](https://apps.ok1cdj.com)).

## Verze

Aktuální verze je v `src/Version.h` a zobrazuje se na obrazovce Nastavení.
Kompletní historie změn je v **[CHANGELOG.md](CHANGELOG.md)**.
