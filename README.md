# MeteoPlaneRadar

**Hodiny, radar letadel, srážkový meteoradar a předpověď počasí na kulatém
dotykovém displeji.** Běží na deskách Waveshare ESP32-S3-Touch-LCD-2.1
a ESP32-S3-Touch-LCD-2.8C a nastavuje se z prohlížeče.

> Vyvíjí **[chiptron.cz](https://chiptron.cz)** a Claude AI.
> English version: **[README_EN.md](README_EN.md)**

---

## Co to umí

| Obrazovka | Co ukazuje | Zdroj |
| --- | --- | --- |
| **Hodiny** | čas, datum, počasí, vteřinový prstenec | Open-Meteo |
| **Letadla** | letadla v okolí, detail letu včetně trasy | adsb.fi, adsb.lol |
| **Meteoradar** | animovaná srážková situace | ČHMÚ nebo RainViewer |
| **Předpověď** | 6 hodin a 3 dny, ovzduší a pyl | Open-Meteo |
| **Nastavení** | jas, orientace mapy, jednotky, jazyk | — |

První čtyři jdou vypnout, Nastavení je dostupné vždy. Rozhraní je česky nebo
anglicky.

## Hardware

**Waveshare ESP32-S3-Touch-LCD-2.1** nebo **ESP32-S3-Touch-LCD-2.8C** —
ESP32-S3R8 (8 MB PSRAM, 16 MB flash), kulatý displej 480×480 s řadičem ST7701,
expandér TCA9554. Dotyk je CST820 (2.1), respektive GT911 (2.8C).
Stačí deska a USB-C kabel, nic se nepájí.

Obě desky jsou pinově totožné a **jedna binárka běží na obou** — firmware se při
startu zeptá dotykového řadiče, kdo je, a podle toho zvolí inicializaci panelu
i časování. Na kterou desku se rozhodl, ukazuje sériový výpis při startu a
stavová tabulka na webu. Kdyby to na vaší desce určilo špatně (typicky vadný
dotykový řadič), dá se to napevno vnutit přes `BOARD_FORCE` v `Config.h`.

---

## První zapnutí

1. Nahrajte firmware (viz níže) a zapněte desku.
2. Připojte se k otevřené síti **`MeteoPlaneRadar`** — na displeji je QR kód.
3. Otevřete `http://192.168.4.1/`, vyberte svou WiFi a uložte.

Přístupový bod zůstane aktivní, dokud síť nezadáte. Potom najdete nastavení na
**`http://meteoplaneradar.local/`** nebo na IP adrese, která je vypsaná na
obrazovce Nastavení.

## Nastavení v prohlížeči

Web běží trvale. Nastavíte v něm polohu (i vyhledáním města), které obrazovky
zobrazovat a jestli se mají střídat, zdroj meteoradaru, jazyk, denní a noční
jas, vzhled hodin, filtry letadel a upozornění na nouzové squawky. Je tam i
dálkové ovládání (přepínání obrazovek a rozsahu bez dotyku displeje), stavová
stránka, export a import nastavení a aktualizace firmwaru.

Většina položek se **ukládá hned po změně**. Tlačítko Uložit zůstává jen pro
polohu, výběr obrazovek a zdroj meteoradaru — ty vyžadují restart zařízení.

### Heslo

Heslo je **nepovinné a ve výchozím stavu žádné není** — aktualizace firmwaru,
import nastavení i tovární reset jsou hned po nahrání přístupné komukoli, kdo se
dostane na adresu zařízení. Nastavíte ho v záložce Systém: pole *Současné heslo*
necháte poprvé prázdné a vyplníte jen *Nové heslo*. Jedna mezera v Novém hesle
ochranu zase zruší. Uživatelské jméno na stránce `/update` je `admin`.

Heslo se **ukládá v čitelné podobě**. Aktualizační stránka používá HTTP Basic a
knihovně se musí předat skutečné heslo, takže otisk tam použít nelze — hashovat
kopii v NVS, když vedle leží totéž otevřeně, by bylo jen divadlo. Chrání to před
domácností, ne před někým, kdo si přečte flash nebo odposlouchává síť.

**Zapomenuté heslo** jde zrušit jedině podržením tlačítka **BOOT při startu
(~3 s)**. To provede tovární reset, takže smaže i WiFi a všechna ostatní
nastavení.

## Ovládání

| Gesto | Akce |
| --- | --- |
| **Přejetí prstem** | změna rozsahu (letadla, meteoradar) |
| **Krátké klepnutí** | výběr letadla / přepnutí den–noc na hodinách |
| **Dlouhý stisk vlevo / vpravo** | předchozí / následující obrazovka |
| **BOOT při startu (~3 s)** | tovární reset |

Na obrazovce Nastavení se řádkem `Nahore` volí, **který světový směr je nahoře**
na radaru letadel — tedy směr, kterým se díváte z okna. Meteoradar se záměrně
neotáčí.

## Meteoradar: ČHMÚ nebo RainViewer

**ČHMÚ** je ostřejší, ale pokrývá jen ČR a okolí — s polohou v zahraničí zůstane
obrazovka prázdná. **RainViewer** pokrývá Evropu i svět. Jeho dlaždice končí na
zoomu 7, takže bližší rozsahy se skládají ze zoomu 7 zvětšeného mocninou dvojky;
obraz je hrubší, ale víc detailu RainViewer nemá. Stahuje se postupně — nejdřív
nejnovější snímek, animace se dotáhne na pozadí. Legenda se přepíná spolu se
zdrojem, protože každý má vlastní paletu.

## Hodiny bez NTP

Čas se bere z hlavičky `Date` odpovědí, které zařízení stahuje tak jako tak.
Neprochází UDP portem 123, nedá se zvlášť zablokovat a nečeká se na něj při
startu. Časové pásmo je v `TZ_INFO` v `Config.h`.

---

## Nahrání firmwaru

**Přes USB** (první nahrání a záchrana): stáhněte `*.merged.bin` z Releases,
připojte desku do konektoru **USB** a nahrajte na
[esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz) v Chrome nebo Edge.

**Přes WiFi**: stáhněte `*.ino.bin` (bez `merged`) a nahrajte na
`http://meteoplaneradar.local/update`. Displej během zápisu zhasne — RGB panel
čte obraz z PSRAM a zápis do flash mu data odřezává. Po dokončení se rozsvítí.

Podrobnosti v [OTA_NAVOD.md](OTA_NAVOD.md).

---

## Pro vývojáře

Arduino IDE, **ESP32 core 3.x**, knihovny: **GFX Library for Arduino**,
**PNGdec**, **ArduinoJson v7**, QRCode (přibalen).
`WebServer`, `DNSServer`, `ESPmDNS`, `Update` a `Preferences` jsou v core.
WiFiManager se nepoužívá od 0.6.0, ElegantOTA od 0.6.2.

Nastavení IDE: ESP32S3 Dev Module, **PSRAM: OPI**, Flash 16 MB QIO,
**Partition Scheme: Custom** (`partitions.csv`, dvě aplikační oblasti pro OTA),
USB CDC On Boot: Enabled. Nebo `arduino-cli compile --profile default MeteoPlaneRadar`.

```
MeteoPlaneRadar.ino   správce obrazovek, dotyk, hlavní smyčka
Config.h              laditelné konstanty
Settings.*            nastavení v NVS + JSON pro web
Lang.*                české a anglické řetězce
Layout.*              pásy obrazovky a hlídání kolizí
WebConfig.* WebPage.h webový server, API, portál, OTA
Net.*                 sdílené HTTPS stahování
Forecast.*            Open-Meteo: předpověď, slunce, ovzduší
RainViewer.*          dlaždicový radar
Screen*.{h,cpp}       jednotlivé obrazovky
```

**Proč se prvky nepřekrývají:** obrazovka si své pásy zabere dřív, než se kreslí
mapa. Cokoli umístěného podle dat (popisky měst, callsigny) si pak musí místo
vyžádat, a když je zabrané, nekreslí se vůbec. Konstanty `LY_*` v `Layout.h`
drží stejné prvky na stejných řádcích napříč obrazovkami.

Sériový výpis 115200 Bd přes konektor **USB**. Totéž bez kabelu je na stavové
stránce webu.

---

## Zdroje dat

Jen pro osobní nekomerční použití — respektujte podmínky poskytovatelů.

**Letadla, registrace a typ:** [adsb.fi](https://adsb.fi) · **trasy:** [adsb.lol](https://adsb.lol) ·
**srážky ČR:** [ČHMÚ](https://opendata.chmi.cz) ·
**srážky svět:** [RainViewer](https://www.rainviewer.com) ·
**počasí, ovzduší, geokódování:** [Open-Meteo](https://open-meteo.com) ·
**poloha podle IP:** [ip-api.com](http://ip-api.com) ·
**mapa:** hranice Natural Earth (public domain), města [GeoNames](https://www.geonames.org) (CC BY 4.0) ·
**čas:** hlavička `Date` (bez NTP)

## Přispěvatelé

- **[Pájeníčko.cz](https://pajenicko.cz)**
  - **Podpora desky Waveshare ESP32-S3-Touch-LCD-2.8C.** Deska se pozná při
    startu podle dotykového řadiče, takže jedna binárka běží na 2.1 i na 2.8C
    a nikdo nemusí vybírat, který soubor nahrát.

## Z čeho projekt vychází

- [ok1cdj/MeteoPlaneRadar](https://github.com/ok1cdj/MeteoPlaneRadar) — fork Ondry OK1CDJ; odtud pochází nápad na RainViewer, obrazovku předpovědi, volitelné obrazovky a jejich střídání
- [CooLajz/waveshare-hodiny](https://github.com/CooLajz/waveshare-hodiny) — Home Assistant dashboard na stejné desce; inspirace pro hodiny, vteřinový prstenec, noční režim a webovou konfiguraci
- [MatixYo/ESP32-Plane-Radar](https://github.com/MatixYo/ESP32-Plane-Radar) — původní radar letadel a zdroj adsb.fi
- [Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar](https://github.com/Selbyl/ESP32-S30Touch-LCD-2.1_Plane-Radar) — port na Waveshare 480×480
- [mylms/ESP-MeteoRadar](https://github.com/mylms/ESP-MeteoRadar) — srážkový meteoradar ČHMÚ

## Licence

**MIT** — viz [LICENSE.txt](LICENSE.txt). Vztahuje se na zdrojový kód i na
binárky z něj přeložené. Od 0.6.2 v projektu není žádná copyleftová závislost.

**Na data se MIT nevztahuje.** **GeoNames**, **ČHMÚ**, **RainViewer** a
**Open-Meteo** vyžadují **uvedení zdroje**; Open-Meteo a adsb.fi mají bezplatné
API určené jen pro nekomerční použití. Podrobnosti jsou v `LICENSE.txt` —
**přečtěte si ho celý, než to nasadíte komerčně.**

Nad rámec licence budeme rádi, když na obrazovce nastavení ponecháte řádek
**chiptron.cz**.

Historie změn: [CHANGELOG.md](CHANGELOG.md). Verze: `MeteoPlaneRadar/Version.h`.
