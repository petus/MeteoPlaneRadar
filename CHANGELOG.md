# Changelog

Všechny podstatné změny v projektu **MeteoPlaneRadar**.
Formát vychází z [Keep a Changelog](https://keepachangelog.com/cs/1.1.0/),
verzování je [semantické](https://semver.org/lang/cs/).

Verze je v jediném místě: `src/Version.h` (`FW_VERSION`). Zobrazuje se na
obrazovce Nastavení, na OTA obrazovce a v sériovém výpisu při startu.
Laditelné konstanty (krok otočení, tolerance výpadků, ladicí výpisy) jsou
pohromadě v `src/Config.h`.

---

## [0.6.1]

### Přidáno
- **APRS: domácí poloha na mapě.** Když je vaše poloha známá (ručně nebo přes IP)
  a padne do aktuálního rozsahu kolem stanice, zobrazí se jako malý **azurový
  bod** — hned vidíte, kde jste vůči stanici. Odlišený od stanice (zelený
  kosočtverec).
- **APRS: hustší mapa a čitelnější popisky.** Obrazovka ukazuje o úroveň víc měst
  než radar letadel (do 50 km i města od 50 tis., dál od 150 tis.) a názvy měst
  jsou ve větším písmu. `EuBorder_DrawCities` má nový nepovinný parametr
  `textSize` (výchozí 1), takže radar letadel zůstává beze změny.

### Změněno
- **Ztišení konzole** (`-DCORE_DEBUG_LEVEL=0` v `platformio.ini`). Zmizí neškodné
  `[E]` hlášky jádra — `setSocketOption … Bad file number` (z TLS spojení) a
  `Wire … Error -1` (z I²C dotyku) — které nemají vliv na funkci a firmware je
  jen zahazoval. Vlastní výpisy (`ADSB:`, `APRS:` …) zůstávají. Vedlejší efekt:
  firmware se zmenšil o ~40 kB (odpadly ladicí řetězce jádra).

## [0.6]

### Přidáno
- **Nová obrazovka: APRS stanice (aprs.fi).** Ukazuje polohu **jedné nastavené
  stanice** na mapě, která je **vycentrovaná na stanici** — rozsah (poloměr
  kolem ní: 25 / 50 / 100 / 200 km) se mění přejetím prstem stejně jako
  u radaru letadel. Dole se barevně zobrazuje **stáří poslední pozice**
  (zeleně ≤ 15 min, žlutě ≤ 1 h, červeně víc), u pohyblivých stanic i rychlost
  a kurz. Znovu využívá projekci, obrys států a města z radaru letadel. Zařazena
  je mezi meteoradar a nastavení (**Letadla → Meteoradar → APRS → Nastavení**).
  Data se stahují po 30 s, při chybě dvojnásobek; při selhání zůstane poslední
  poloha na displeji.
- **Synchronizace času přes NTP (vráceno).** APRS potřebuje vědět, jak jsou data
  stará, takže se `configTzTime()` vrátil nad `setup()` (v 0.5.3 byl NTP odebrán
  jako nepotřebný — přesně pro tento případ tam zůstala poznámka „kdyby přibyly
  hodiny"). Běží **neblokujícím způsobem**: `time()` se stane platným pár sekund
  po připojení, do té doby obrazovka píše `cas: ?`. HTTPS spojení dál jedou přes
  `setInsecure()`, takže se neověřuje platnost certifikátů.
- **Pole ve WiFi portálu** pro *APRS volací znak stanice* a *aprs.fi API klíč*
  (bezplatný, z účtu na aprs.fi → *My Account → API key*). Ukládají se do NVS,
  prázdné pole stávající hodnotu nepřepíše. Bez znaku obrazovka jen vypíše výzvu
  k nastavení.
- **Podpora PlatformIO.** V kořeni je `platformio.ini`, který zrcadlí
  `sketch.yaml` (ESP32‑S3, OPI PSRAM, 16 MB QIO flash, custom `partitions.csv`,
  USB CDC on boot) a připíná stejný ESP32 core **3.0.7** i verze knihoven.
  Používá komunitní platformu **pioarduino** (oficiální `espressif32` zatím vozí
  jen core 2.x); `ESPAsyncWebServer`/`AsyncTCP` jsou přes `lib_ignore` vyřazené
  (ElegantOTA běží synchronně a nepotřebuje je). Překlad `pio run`, nahrání
  `pio run -t upload`, monitor `pio run -t monitor`.
- Konstanty `APRS_API_BASE`, `APRS_RANGES_KM` a `APRS_PERIOD_MS` (30 s)
  v `src/Config.h`; nové klíče NVS `aprsCall`, `aprsKey`, `rngA`.

### Opraveno
- **Sladění se staršími verzemi knihoven připnutými v `sketch.yaml`.**
  `Canvas16::flush()` má nově podpis `flush(void)` (odpovídá GFX 1.4.9) a
  callback `pngDraw` v meteoradaru vrací `void` (PNGdec 1.0.1) — s těmito
  připnutými verzemi projekt jinak neprošel překladem. Chování se nemění.

## [0.5.4]

### Opraveno
- **Displeji po chvíli zčernal obraz a pomohl jen restart.** Podsvícení přitom
  svítilo dál a deska běžela normálně. Na vině byla automatická obnova
  dotykového řadiče přidaná v 0.5.3: za zaseknutý řadič považovala i to, když
  čip jen usne a přestane odpovídat, což je jeho běžné chování. Resetovala ho
  proto pořád dokola — a protože reset vede přes obvod, který drží i napájení
  displeje, dřív nebo později jeden zápis skončil špatně a panel zhasl.
  Obnova je nově vypnutá (`TOUCH_RECOVERY 0` v `Config.h`).

### Přidáno
- **Hlídač displeje.** Sleduje, jestli panel opravdu vykresluje snímky. Když se
  zastaví, zkusí ho oživit, a pokud se to nepovede, deska se sama restartuje —
  místo aby zůstala černá až do odpojení napájení.
- **Hlídač I/O expandéru.** Průběžně kontroluje, že obvod držící napájení
  a reset displeje má nastavené to, co má, a případný rozdíl opraví.

## [0.5.3]

### Opraveno
- **Gesto už nekončí na prvním prázdném vzorku.** CST820 běžně vynechá vzorek
  uprostřed tahu a od verze 0.5.1 se navíc zahazují vadná čtení — obojí se
  tvářilo jako zvednutí prstu. Jeden swipe se tak rozpadl na několik krátkých
  klepnutí, která pak zavírala detail letadla nebo klepala do mapy. Nově se
  vyžaduje `TOUCH_RELEASE_MS` (60 ms) souvislého ticha. Ověřeno simulací:
  swipe s výpadkem 20 ms dřív dal *dvě klepnutí a žádný swipe*, teď jeden swipe.
- **Čas u snímků meteoradaru byl v létě o hodinu vedle.** Posun UTC → místní čas
  se počítal z *aktuálních* hodin. Dokud nedorazila odpověď z NTP, ležely
  v roce 1970, tedy v lednu, takže se použil CET místo CEST. Nově se posun
  odvozuje z data *toho snímku* (název souboru nese `YYYYMMDDHHMM` v UTC), takže
  vychází správně nezávisle na hodinách — včetně nocí, kdy se přechází mezi
  letním a zimním časem. Popisek „nyni / −X min" se počítá z pořadí snímku
  a nebyl ovlivněn nikdy.
- **Jas se zapisoval do flash při každém pohybu slideru.** Přetažení přes celou
  šířku znamenalo desítky zápisů do NVS. Nově prochází stejným odloženým
  zápisem (~2 s klidu) jako rozsah a orientace.
- **Bílý záblesk při startu.** Podsvícení se rozsvěcelo dřív, než byl vykreslen
  první snímek, takže bylo vidět náhodný obsah paměti panelu. Rozsvítí se až za
  prvním `flush()`.

### Přidáno
- **Zotavení dotykového řadiče.** Po `TOUCH_REINIT_BAD` (40) vadných čteních za
  sebou se CST820 resetuje. Dřív po zámrzu I2C přestal dotyk fungovat až do
  odpojení napájení.
- **Kontrola volné paměti před TLS.** Handshake potřebuje ~45 kB *interní* RAM;
  když chybí, mbedTLS to hlásí jen jako `HTTP -1`. Nově se poll přeskočí
  a důvod se vypíše (`NET_MIN_HEAP`).
- **Ošetření selhání displeje.** `ST7701_Init()` vrací `bool` a kontroluje
  návratové kódy SPI i `esp_lcd_new_rgb_panel` — nejčastější příčina (PSRAM
  není v IDE nastavená na OPI) se teď vypíše místo černé obrazovky bez stopy.
  `LCD_Flush()` a `LCD_DrawBitmap()` navíc nesáhnou na neinicializovaný panel.
- **Důvod restartu a volná paměť v sériovém výpisu** při startu — panic,
  watchdog a brownout jsou v hlášení chyb rozlišitelné na první pohled.
- Timeout konfiguračního portálu je nově v `Config.h` (`PORTAL_TIMEOUT_S`).

### Odebráno
- **NTP klient a systémový čas.** Po opravě časů výše je nepotřebuje nic:
  popisek HH:MM se odvozuje z názvu snímku, „nyni / −X min" z jeho pořadí
  v animaci a všechna HTTPS spojení jedou přes `setInsecure()`, takže se
  neověřuje ani platnost certifikátů. Zmizelo čekání při startu i varování
  „NTP neodpovedel". Kdyby na displeji někdy přibyly hodiny, vrátí se
  `configTzTime()` zpátky nad `setup()`.
- Konstanty `NTP_SERVER`, `NTP_WAIT_MS` a `NTP_RETRY_MS` z `Config.h`.

### Změněno
- **Časová zóna se nastavuje explicitně** (`setenv("TZ", ...)` + `tzset()`
  v `setup()`). Dřív to byl vedlejší efekt `configTzTime()`; bez něj by
  `localtime_r()` v `CHMU.cpp` tiše vracelo UTC a popisky by byly o hodinu
  nebo dvě vedle. `TZ_INFO` v `Config.h` proto zůstává.

## [0.5.2]

### Změněno
- **Otočení mapy se nastavuje srozumitelněji.** Řádek v Nastavení se jmenuje
  `Nahore` a udává, **který světový směr je nahoře na displeji** — tedy směr,
  kterým se díváte z okna. Dřív se zadávalo „o kolik mapu otočit", což je něco
  jiného: pro výhled na východ bylo potřeba nastavit 270°, a při špatné hodnotě
  mapa působila zrcadlově. Nově se zadává rovnou `V`.
  Hodnota se zobrazuje jako zkratka světové strany (`S`, `SV`, `V`, `JV`, `J`,
  `JZ`, `Z`, `SZ`), tlačítko `+` jde po směru hodinových ručiček.
  Uloženo je pod novým klíčem v NVS, takže **stará hodnota se po aktualizaci
  nepřenese** a orientace začíná na severu nahoře — nastavte si ji prosím
  znovu (jedno klepnutí).

## [0.5.1]

### Opraveno
- **Detail letadla se zavíral sám od sebe.** Skutečnou příčinou nebyla data
  z adsb.fi, ale **vadná čtení z dotykového řadiče**. Když I2C přenos selže na
  úrovni dat, CST820 vrátí samé `0xFF` — a to se dekódovalo jako „15 bodů na
  souřadnicích (4095, 4095)", tedy jako platné klepnutí mimo panel, které
  detail zavřelo. Ve výpisu uživatele tomu odpovídá **každé** samovolné
  zavření. Nově se surová data ověřují: zahodí se samé `0xFF`, nesmyslný počet
  bodů (CST820 je jednodotykový) i souřadnice mimo displej. Počet zahozených
  čtení se při zapnutém `TOUCH_DEBUG` vypisuje jednou za sekundu.

## [0.5]

### Přidáno
- **Otočení mapy** — v Nastavení přibyl řádek `Otoceni` s tlačítky `−` / `+`,
  krok **45°** (osm poloh). Slouží k tomu, aby letadlo viděné z okna bylo na
  displeji ve stejném směru. Otáčí se **projekce**, ne displej, takže se spolu
  s letadly správně otočí i obrys států, města a ikony letadel.
  Vedle ovládání je **kompasový náhled** (kroužek s ryskou a písmenem S), takže
  je nastavení vidět hned bez přepínání na radar.
  **Meteoradar se záměrně neotáčí** — srážková mapa se čte severem nahoru a
  orientaci v ní drží obrys ČR.
  Tlačítko `+` otáčí mapou **po směru hodinových ručiček** (sever putuje nahoru
  → vpravo nahoru → vpravo), `−` opačně.
  Krok se dá změnit přes `MAP_ROT_STEP_DEG` v `Config.h` — musí ale dělit 90,
  jinak přestanou být dosažitelné přesný východ a západ.
- **Značky světových stran** (S, V, J, Z) po obvodu radaru letadel; otáčejí se
  spolu s mapou.

### Změněno
- **Uživatelské rozhraní je celé česky** (bez diakritiky — vestavěný font umí
  jen ASCII). Například „Letadel: 12", „Nastaveni", „Jas", „Poloha",
  „Aktualizace FW", v detailu letadla „Vyska", „Rychlost", „Kurz", „Stoupani".
- Rozvržení obrazovky Nastavení zhuštěno, aby se vešel řádek s otočením.

### Opraveno
- **Trhající se pás uprostřed displeje.** Měření ukázalo, že kopie celého
  snímku do framebufferu panelu trvala **28 ms**, zatímco jeden snímek trvá
  34 ms — zápis a vykreslování se tedy pohybovaly skoro stejnou rychlostí a
  někde uprostřed obrazovky se předjely. Nově má panel **dva framebuffery** a
  kreslí se rovnou do toho, který zrovna není vidět (`num_fbs = 2`, bounce
  buffery pryč). Driver pozná svůj vlastní buffer a místo kopírování jen
  přepne DMA, takže se ta 28ms kopie neprovádí vůbec a zátěž sběrnice PSRAM
  výrazně klesne.
- **Detail letadla se už nezavírá sám.** adsb.fi občas letadlo v jednom stažení
  vynechá a v dalším ho zase pošle; dřív stačil jeden takový výpadek a panel se
  zavřel. Nově se tolerují **dvě po sobě jdoucí chybějící stažení**
  (`DETAIL_GRACE_POLLS` v `Config.h`), během nichž panel zůstane otevřený
  s posledními známými hodnotami a poznámkou „signal ztracen".

### Diagnostika
- Volitelné **měření doby překreslení** (`FLUSH_DEBUG` v `Config.h`) — jednou
  za sekundu vypíše min/poslední/max dobu jednoho flushe. Slouží k ověření,
  jestli se stíhá překreslit do jednoho snímku.
- Volitelné **ladicí výpisy dotyku** (`TOUCH_DEBUG` v `Config.h`, výchozí
  zapnuto). Do sériové linky se vypisuje každé dokončené gesto i každá změna
  výběru letadla **včetně důvodu**, proč se detail zavřel (`tap mimo panel`,
  `letadlo zmizelo z dat`, `dlouhy stisk`). Podle toho jde odlišit falešný
  dotyk od výpadku dat.

## [0.4]

> ### ⚠️ Upozornění k aktualizaci na 0.4
> Verze 0.4 mění **rozdělení paměti** (dvě aplikační oblasti, aby bylo kam
> nahrát bezdrátovou aktualizaci). Proto se na ni **z verze 0.3 a nižší nedá
> přejít přes OTA** — je nutné jednou nahrát soubor `*.merged.bin` přes
> [esp32flasher.chiptron.cz](https://esp32flasher.chiptron.cz) a USB kabel.
> Od 0.4 dál už aktualizace probíhá bezdrátově.

### Přidáno
- **OTA aktualizace firmware přes WiFi** (ElegantOTA). V Nastavení přibylo
  tlačítko „Firmware update": deska vytvoří AP `MeteoPlaneRadar`, na displeji
  ukáže QR kód a firmware se nahraje z prohlížeče na `192.168.4.1/update`.
  Vyžaduje OTA rozdělení flash (`src/partitions.csv`, dva app sloty).
- **Zapamatování stavu UI** — poslední rozsah (zvlášť pro letadla a meteoradar)
  a naposledy zobrazená obrazovka se ukládají do NVS a obnoví se po restartu.
  Zápis je odložený (~2 s po poslední změně), aby swipování nezatěžovalo flash.
- **Zobrazení verze firmwaru** na obrazovce Nastavení (pod titulkem), na OTA
  obrazovce a v sériovém výpisu. Nová sdílená hlavička `src/Version.h`.
- **Sjednocení nastavení** do `src/Config.h` — časová zóna, výchozí poloha,
  rozsahy, intervaly stahování, název AP a limity na jednom místě.
- **CI build na GitHubu** — každý push se automaticky zkusí přeložit.
- Tento `CHANGELOG.md`, `.gitignore`, `sketch.yaml` a `LICENSE` v kořeni.

### Změněno
- Během OTA se na displeji ukáže jen „Probiha aktualizace…" a **podsvícení se
  vypne** po dobu zápisu. Průběh v procentech se nevykresluje: RGB panel čte
  obraz z PSRAM průběžně a zápis do flash mu data odřezává, takže by obraz
  poskakoval. Procenta jsou vidět v prohlížeči.
- Historie verzí se přesunula z hlavičky `.ino` sem.

### Opraveno
- Meteoradar se při každém vstupu na obrazovku zbytečně znovu dekódoval
  (všech 6 PNG). Nově se přepočítá jen při skutečné změně rozsahu.

## [0.3]

### Změněno
- **Robustní stahování ADS-B.** Celé HTTP tělo se načte do znovupoužitelného
  PSRAM bufferu a parsuje se až kompletní (kontrola utnutí proti
  `Content-Length` + jeden retry), místo parsování přímo z TLS streamu. Tím
  zmizely občasné chyby stahování „IncompleteInput".
- Parsování používá **ArduinoJson filtr** (nechá jen pole, která se používají),
  takže dokument zůstává malý bez ohledu na objem dat.
- Pozemní letadla se zahazují už při parsování.
- **Perioda stahování podle rozsahu** (5 / 10 / 15 s) a po neúspěšném stažení
  dvojnásobek, aby se šetřilo bezplatné API adsb.fi.
- Limit letadel `ADSB_MAX` zvýšen ze 40 na **100**.
- **Ovládání:** dlouhý stisk přepíná obrazovky směrově (levá půlka =
  předchozí, pravá = následující, s přetočením dokola) místo slepého cyklení.

### Opraveno
- Při chybě stahování zůstane poslední platný snímek — radar už nebliká na
  prázdno.

## [0.2]

### Přidáno
- První veřejná verze: radar letadel (adsb.fi) + animovaný srážkový meteoradar
  ČHMÚ na kulatém displeji 480×480.
- Oprava problikávání pixelů uprostřed displeje: jedno plátno v PSRAM a jediný
  přenos snímku synchronizovaný s VSYNC (`num_fbs=1` + bounce buffery,
  pixel clock 8 MHz).
