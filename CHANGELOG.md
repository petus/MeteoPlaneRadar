# Changelog

Všechny podstatné změny v projektu **MeteoPlaneRadar**.
Formát vychází z [Keep a Changelog](https://keepachangelog.com/cs/1.1.0/),
verzování je [semantické](https://semver.org/lang/cs/).

Verze je v jediném místě: `MeteoPlaneRadar/Version.h` (`FW_VERSION`). Zobrazuje se na
obrazovce Nastavení, na webové stránce a v sériovém výpisu při startu.
Laditelné konstanty (krok otočení, tolerance výpadků, ladicí výpisy) jsou
pohromadě v `MeteoPlaneRadar/Config.h`.

---

## [Nevydáno]

### Přidáno

- **Karty — vlastní obrazovky popsané souborem JSON.** Nová obrazovka Karty
  ukazuje až čtyři hodnoty stažené z adresy, kterou určuje karta: malý soubor
  s nadpisem, adresou a cestami k hodnotám v odpovědi. Karta je popis, ne
  program — rozvržení na kulatém displeji dělá firmware. Karet může být osm a každá je
  samostatná obrazovka: střídá se s ostatními ve stejném intervalu. Nahrávají a mažou se na webu na
  záložce Obrazovky (s heslem správce, je-li nastavené). Ukázka s kurzy ČNB
  a popis formátu: [cards/README.md](cards/README.md).

  Obrazovka je ve výchozím stavu vypnutá a vypnutá nic nestahuje.

- **Obchod s kartami.** Nová záložka Obchod na webu nabídne karty, které jde
  přidat jedním klepnutím. Zařízení si stáhne seznam `cards/index.json`,
  ověří jeho podpis (ECDSA P-256) proti klíči ve firmwaru a každou kartu
  porovná s otiskem SHA-256 ze seznamu. Nepodepsaný, změněný nebo starší
  seznam odmítne. Ruční nahrání karty funguje dál a podpis nepotřebuje.
  Seznam vyrábí a podepisuje `tools/make_index.py`. Popis:
  [cards/README.md](cards/README.md#obchod-s-kartami).

- **Karty se skriptem.** Karta s položkou `script` si displej kreslí sama
  malým programem v jazyce Berry: zařízení dál stahuje adresu z karty a skript
  z odpovědi nakreslí, co chce — tabulku, prstenec, vlastní rozvržení. Skript
  má k dispozici jen kreslení (`g`), data své karty (`net`) a parametry a čas
  (`cfg`); k souborům, nastavení ani k síti nemůže. Skripty jsou ve výchozím
  stavu vypnuté. Jedno vykreslení smí trvat 500 ms a zabrat 96 kB; po třech
  selháních se skript karty vypne, a pokud se zařízení restartuje uprostřed
  skriptu, vypne se při dalším startu (bezpečný režim). Ukázky: kurzovní
  lístek a „Den“. Popis: [cards/README.md](cards/README.md#karty-se-skriptem).

### Změněno

- Karty se ukládají do LittleFS v oddílu `spiffs`, který v `partitions.csv`
  byl od začátku a nic ho nepoužívalo. Při prvním startu po aktualizaci se
  oddíl jednorázově naformátuje (ještě před rozsvícením displeje).
- Index obrazovky Nastavení se posunul z 6 na 7, Karty mají 6 — stejný postup
  jako u energetických obrazovek v 0.7.0. Uložená nastavení zůstávají platná.
- `/api/config` a export nastavení mají v `screens` nový klíč `cards`,
  `/api/status` nový řádek `cards`, `/api/ha` může v `screen` vrátit `cards`.
  Nové jsou `/api/cards` (GET, POST) a `/api/cards/delete` (POST).
- Obchod přidává `/api/store` (GET) a `/api/store/install` (POST, s heslem
  správce, je-li nastavené). Firmware je s ním asi o 12 kB větší; ověřování dělá
  mbedTLS, které je v jádře ESP32 kvůli TLS, žádná knihovna nepřibyla.
- Karty se skriptem přidávají `/api/scripts` (POST) a do `/api/cards` pole
  `scripts`, `script`, `off` a `why`. Interpret Berry (licence MIT) je
  přibalený ve složce `MeteoPlaneRadar/src/berry`, není to knihovna
  k instalaci. Firmware je s ním o dalších asi 100 kB větší. Soubor karty
  smí mít nově 8192 bajtů místo 2048.
- **Tečky obrazovek nahoře jdou vypnout a ve výchozím stavu jsou skryté.**
  Každá karta je samostatná obrazovka a má svou tečku, takže s několika
  kartami byla řada dlouhá a málo říkala. Zapíná se na webu na záložce
  Obrazovky volbou „Zobrazovat tečky obrazovek nahoře“ a změna se projeví
  hned, bez restartu. V `/api/config` a v exportu nastavení je to klíč
  `screenDots`.
- **Nová ukázková karta se skriptem: hlídání webu chiptron.cz.** Každých
  5 minut stáhne malou odpověď webu; zelená fajfka, když odpovídá, oranžová
  po prvním nezdaru, červený křížek po třech nezdarech za sebou. Je v obchodě
  (seznam má číslo 4). Popis: [cards/README.md](cards/README.md#ukázka-hlídání-webu).

### Vyzkoušeno

- Karty, obchod, karty se skriptem a střídání karet běžely 12. 11. 2026 na
  desce Waveshare ESP32-S3-Touch-LCD-2.1 po aktualizaci z 0.8.5
  (aktualizace přes web tam i zpět, obchod s ověřením podpisu, všechny
  ukázkové karty). Nevyzkoušené: deska 2.8C, dlouhý provoz, osm karet,
  heslo správce.

---

## [0.8.5]

### Změněno

- **Vyhlazené písmo na všech obrazovkách.** Výběr písma (Inter, Poppins,
  Inter tenké, pixelové) už neplatí jen pro hodiny, ale pro celý displej:
  letadla a jejich detail, meteoradar, předpověď, cenu elektřiny, výrobu,
  nastavení i obrazovku pro připojení k WiFi. Na webu je volba na záložce
  Vzhled jako „Písmo (všechny obrazovky)“.

  Rozložení obrazovek se nemění: písmena jsou stejně vysoká jako u původního
  písma, stojí na stejné účaří a text nikdy není širší, než by byl původní
  (kdyby byl, mezery se o kousek zúží). Všechno tedy zůstává tam, kde bylo.

- **Diakritika na displeji.** Vyhlazená písma mají české znaky, takže displej
  teď píše „Předpověď“, „Výška“, „Út“, „října“ místo „Predpoved“, „Vyska“,
  „Ut“, „rijna“. Písma obsahují i další evropské znaky, takže se správně
  zobrazí i názvy letišť jako „Zürich“ nebo „Kraków“. S pixelovým písmem
  zůstává všechno bez diakritiky jako dřív.

  Diakritiku dostaly i texty, které nejsou v tabulce překladů: „Jazyk:
  čeština“, „v okně“, „Kč/kWh“, „ČHMÚ“, obrazovka připojení k WiFi a hláška
  při továrním resetu.

- **Nejmenší písmo je tučnější.** Zkratky měst, legenda meteoradaru a další
  drobné popisky se kreslí silnějším řezem téhož písma, aby byly i v osmi
  pixelech dobře čitelné.

### Opraveno

- **Legenda meteoradaru ČHMÚ měla špatné hodnoty u čtyř nižších barev.**
  Podle barevné škály ČHMÚ, ověřené na skutečných snímcích, je modrá 12 dBZ
  (ne 20), zelená 24 (ne 32), žlutá 36 (ne 40) a oranžová 44 (ne 46). Hodnoty
  v mm/h jsou přepočtené stejně jako dřív. Legenda RainVieweru se nemění.

---

## [0.8.4]

### Změněno

- **Šetrnější k přetíženému API letadel.** Když adsb.fi neodpoví nebo vrátí
  chybu, zařízení se dřív zeptalo hned znovu a pak dál každých 10–30 sekund.
  Teď každá další chyba v řadě prodlouží pauzu na dvojnásobek (10 s, 20 s,
  40 s … až 5 minut) a pauza platí pro všechno najednou: radar letadel,
  hlídání letadel poblíž i Home Assistant. Na odpověď **429** (příliš mnoho
  dotazů) nebo chybu serveru (5xx) se už v témže pokusu znovu neptá, a když
  server pošle `Retry-After`, počká tak dlouho, jak říká. První úspěšná
  odpověď všechno vrátí do normálu.

- **Stará letadla nevypadají jako živá.** Když minutu nepřijdou nová data,
  letadla na radaru zešednou a místo počtu se ukáže „stará data: X min“
  i se zbývající pauzou. Po deseti minutách se letadla přestanou kreslit
  úplně („bez dat o letadlech“). Ze starých dat se nespustí ani upozornění na
  letadlo poblíž a Home Assistant dostane počet 0, `stale: true` a stáří dat
  v `data_age_s`.

- **Střídání obrazovek se kvůli výpadku nezastaví.** Otevřený detail letadla
  dřív střídání držel. Teď ho drží, jen dokud jsou data čerstvá; když minutu
  nepřijdou nová, detail se zavře a obrazovky se střídají dál. Stahování
  během pauzy navíc nic neblokuje, takže displej nečeká na server, který
  neodpovídá.

---

## [0.8.3]

### Opraveno

- **Výroba ČR ukazovala jen „Načítám“.** Zařízení se ptalo energy-charts na
  poslední 4 hodiny, protože data obvykle chodí s hodinovým až dvouhodinovým
  zpožděním. 25. 10. 2026 byl ale nejnovější vzorek pro Česko starý jedenáct
  hodin, server na prázdné okno odpovídal `404 no content available` a
  obrazovka se nikdy nenačetla. Když teď v krátkém okně nic není, zkusí se
  12 a pak 36 hodin. Obrazovka pak ukáže nejnovější dostupný vzorek i s jeho
  časem.
- **Čitelnější chyby v sériovém výpisu.** Při chybě HTTP se vypíše i adresa
  dotazu a začátek odpovědi serveru, ne jen kód.

---

## [0.8.2]

### Přidáno

- **Vysvětlivky ikon letadel ve webovém nastavení.** Na záložce Letadla je
  na konci nová karta se všemi ikonami a tím, co znamenají: úzkotrupé, širokotrupé,
  čtyřmotorové, regionální, business jet, turbovrtulové, malá letadla,
  vojenská transportní, těžká nákladní, stíhačky, kluzáky, konvertoplány,
  vrtulníky, balony, drony, nezjištěný typ a neznámý směr letu — vždy
  s příklady typů. Ikony se na stránce kreslí ze stejných tvarů jako na
  displeji, takže se nemůžou rozejít.

- **Uvedení zdroje ikon.** Rozdělení typů letadel do ikon vychází ze schématu
  [ADS-B Radar](https://adsb-radar.com/help/icons.html). Jejich ikony jsou
  volně k použití pod podmínkou odkazu zpět. Projekt jejich soubory nepoužívá
  (tvary kreslí firmware sám), odkaz ale uvádí: v panelu Zdroje dat na webu,
  u vysvětlivek ikon, v README a v LICENSE.txt.

---

## [0.8.1]

### Opraveno

- **Názvy měst na meteoradaru jsou čitelné i nad srážkami.** Žlutá a
  světle zelená oblast nebo bledé odstíny z ČHMÚ masked byly stejně světlé jako
  bílé písmo měst a název v nich zmizel. Teď se před nakreslením popisku
  zkontroluje, co pod ním na displeji je: když je pozadí příliš světlé, popisek
  i tečka města se nakreslí **černě** s tenkým světlým okrajem, takže se dají
  přečíst i tehdy, když leží napůl na srážkách a napůl na černém podkladu. Nad
  tmavým podkladem zůstávají bílé jako dřív. Na radaru letadel je mapa černá,
  takže tam se nic nemění.

---

## [0.8.0]

### Přidáno

- **Meteoradar jen při bouřce.** Nová volba u automatického střídání:
  meteoradar se do střídání dostane jen tehdy, když se k zařízení blíží
  srážky. Každých pět minut se stáhne nejnovější snímek ČHMÚ a zkontroluje se,
  jestli je v zadané vzdálenosti odraz aspoň zadané síly. Výchozí je **20 dBZ
  do 50 km**, obojí jde změnit ve webovém nastavení (záložka Obrazovky).

  Funguje to i tehdy, když je obrazovka Meteoradar jinak vypnutá: bouřka ji
  přidá a po dvou klidných snímcích po sobě zase zmizí, aby neblikala sem a
  tam. Když je meteoradar zrovna na displeji se zdrojem ČHMÚ, hlídání použije
  snímek, který už je stažený, a nestahuje ho podruhé.

  Hlídá se jen podle ČHMÚ, i když obrazovka používá RainViewer: je to jeden
  obrázek se známou barevnou škálou, ne tucet dlaždic. Mimo pokrytí ČHMÚ to
  stavová stránka napíše rovnou, místo aby předstírala jasnou oblohu.

- **Letadlo poblíž.** Stejný princip pro letadla. Obrazovka Letadla se pak ve
  střídání sama neobjevuje, ale jakmile se letadlo přiblíží na zadanou
  vzdálenost (**výchozí 5 km**), zařízení na ni přepne, rovnou otevře detail
  toho letadla a po zadané době (**výchozí 15 s**) se vrátí tam, kde bylo.
  Rozsah radaru se při tom zvolí tak, aby se letadlo vešlo, a pak se vrátí ten
  váš.

  Každé letadlo se ukáže jednou (pamatuje si ho 15 minut), takže letadlo
  kroužící nad domem střídání neruší pořád dokola. Filtry výšky a callsignu
  platí i tady. Dotyk během ukázky ji ukončí a zůstane se na letadlech.
  Funguje i bez automatického střídání.

  Když obrazovka Letadla není na displeji, zařízení se adsb.fi ptá jen na malý
  okruh (vzdálenost upozornění, případně dohled z okna) každých 8 sekund.

- **Výhled z okna.** Na záložce Letadla se zadá směr okna (S, SV, V … SZ),
  šířka výhledu a dohled. Letadlo, které je ve výhledu, má při upozornění
  přednost před bližším letadlem, které z okna vidět není — letadlo 3 km za
  domem je blíž než to 9 km před oknem, ale uvidíte jen to druhé. Výseč se
  zakreslí tenkými čarami do radaru letadel.

- **Tři WiFi sítě.** Zařízení si pamatuje až tři sítě a dají se zapsat ručně ve
  webovém nastavení, i když zrovna nejsou v dosahu — síť v kanceláři se tak
  nastaví ještě doma. Při startu se zkusí ta nejsilnější z dosažitelných, pak
  ostatní (i skrytá síť, kterou sken nevidí). Když spojení spadne, zařízení
  minutu zkouší obnovit tu svou a pak projde ostatní uložené.

  Původní síť zůstává uložená pod stejným klíčem, takže aktualizace nic
  nerozbije. Hesla stránka nikdy nedostane, jen informaci, že je nějaké
  uložené; prázdné pole hesla ho nechá beze změny.

- **Home Assistant.** Nová adresa `http://meteoplaneradar.local/api/ha` vrací
  jeden JSON se vším, co zařízení ví: teplota, vítr, srážky, bouřka v okolí
  (nejsilnější odraz, vzdálenost), nejbližší letadlo (callsign, typ, výška,
  vzdálenost, jestli je v okně), počet letadel, cena elektřiny a podíl OZE.
  Home Assistant si to sám stahuje integrací **RESTful**, žádné MQTT ani
  doplněk není potřeba. Na nové záložce Home Assistant je adresa a hotový
  kousek `configuration.yaml` ke zkopírování.

  Ve výchozím stavu je to vypnuté. Dokud je to zapnuté, běží i hlídání bouřky
  a letadel poblíž (letadla jen jednou za 30 s), aby čísla byla živá.

  > **⚠ Nevyzkoušeno.** Tahle funkce nebyla ověřena se skutečným Home
  > Assistantem — formát odpovídá dokumentaci integrace RESTful, ale nikdo na
  > zařízení zatím Home Assistant nenamířil. Pokud něco nesedí, dejte vědět.

- **ČHMÚ masked.** Třetí zdroj meteoradaru vedle ČHMÚ a RainVieweru. Je to
  tentýž kompozit maximální odrazivosti, ale ČHMÚ ho porovná s měřením ve
  výšce 2 km: plnou barvou jsou jen místa, kde srážky opravdu dopadají na zem,
  zbytek (srážky, které se cestou vypaří, kovadliny vzdálených bouřek) je
  bledší. Na otázku „bude u nás pršet?“ odpovídá přesněji, pro sledování
  bouřek je lepší plný ČHMÚ. Rozdíl je vysvětlený i ve webovém nastavení.

- **Vlastní barvy výšek letadel.** Tři hladiny a čtyři barvy na záložce
  Letadla. Výchozí jsou přesně ty dosavadní (červená pod 2 km, oranžová 2–6
  km, žlutá 6–10 km, modrá nad 10 km), takže dokud se do nastavení nesáhne,
  nic se nemění. Legenda na displeji se přizpůsobí.

### Změněno

- **Hodiny mají hezčí písmo.** Číslice byly vestavěný font 5×7 zvětšený
  osmkrát, tedy schody z kostek 8×8 pixelů. Teď jsou to skutečné obrysy písma
  vykreslené v té velikosti, ve které se zobrazují, se šestnácti stupni
  vyhlazení. Na výběr je **Inter** (výchozí), **Poppins**, **Inter tenké** a
  původní pixelové. Číslice mají všechny stejnou šířku, takže čas při změně
  minuty neposkakuje.

  Vyhlazené písmo dostalo i datum, teplota, srážky a vítr na hodinách — a
  protože fonty obsahují české znaky, datum je konečně „25. října“, ne
  „25. rijna“. Fonty generuje `tools/make_fonts.py`, zabírají asi 190 kB
  flash.

- **Ikony letadel podle typu**, podle schématu
  [adsb-radar.com](https://adsb-radar.com/help/icons.html). Vrtulník má rotor,
  kluzák dlouhá tenká křídla, A380 je viditelně větší než Cessna. Typ se pozná
  z kódu typu z adsb.fi (A20N, B738, EC35…); u neznámého typu z kategorie,
  kterou vysílá každý transpondér (A1 lehké … A7 vrtulník, B1 kluzák). Co nejde
  zařadit, má dosavadní šipku.

  Seznamy typů z té stránky jsou převzaté s opravou několika zjevných chyb
  (737 v dlaždici B767, A350 mezi čtyřmotorovými, MD-90 mezi vrtulníky,
  seznam MD-11 zkopírovaný z ERJ). Žádné soubory ikon z webu se nepřebírají,
  tvary kreslí firmware sám.

- **Stav zařízení** na webu má dva nové řádky: bouřka a letadla poblíž.
- **Dálkové ovládání** na webu zvýrazňuje obrazovku, která je opravdu na
  displeji, i když na ni přepnulo automatické střídání nebo upozornění.
- `sketch.yaml` uvádí verze, se kterými se 0.8.0 skutečně překládá (core
  3.3.10, GFX 1.6.9, PNGdec 1.1.7). Dříve uvedené verze už ke kódu neseděly.

### Poznámky

- Všechny nové limity (dBZ, km, sekundy, okno, barvy, hladiny, písmo) se
  ukládají do NVS, přežijí restart a jsou v exportu i importu nastavení.
  WiFi sítě a hesla v exportu záměrně nejsou, stejně jako dosud.
- Převod barvy snímku ČHMÚ na dBZ vychází z barevné škály ČHMÚ
  (`scl-dbzmmh.png`). Na skutečných snímcích (plném i masked z 18. 10. 2026)
  barvy 4–36 dBZ přesně sedí a bledé odstíny z masked varianty se správně
  nepočítají; silnější barvy (40 dBZ a víc) zatím na reálné bouřce ověřené
  nejsou.

---

## [0.7.0]

### Přidáno

- **Cena elektřiny.** Nová obrazovka se spotovou cenou z denního trhu OTE.
  Den je prstenec, půlnoc nahoře, ručička ukazuje na aktuální blok. Zelená
  strana je ta, kdy pustit pračku, červená ta, kdy ne.

  Ceny jsou čtvrthodinové, tedy v blocích, ve kterých se doopravdy obchoduje.
  Uprostřed svítí cena právě probíhající čtvrthodiny, pod ní nejlevnější a
  nejdražší blok dne i s časem.

  Klepnutí na prstenec vypíše cenu vybraného bloku, klepnutí doprostřed přepne
  mezi dneškem a zítřkem. Přejetí prstem dělá totéž. Ceny na zítřek vycházejí
  kolem 14:00, do té doby je k dispozici jen dnešek.

  Zdrojem je **spotovaelektrina.cz**, zdarma a bez registrace.

  Ceny platí jen pro Česko a jsou v korunách. Když nastavíte polohu mimo ČR,
  obrazovka se sama vypne a zařízení si o data přestane říkat. Zapnout jde
  zpátky ručně, blízko hranic to může dávat smysl.

  Číslo na displeji je burzovní cena, ne to, co platíte. Distribuci, regulované
  složky a marži obchodníka si doplníte na nové záložce Energie. Dokud tam
  necháte nuly, displej ukazuje čistou burzu a napíše to pod číslo.

- **Výroba ČR.** Druhá nová obrazovka. Prstenec ukazuje, z čeho se právě
  vyrábí elektřina, uprostřed je podíl obnovitelných zdrojů, spotřeba a saldo
  přeshraničních toků.

  Zemi si vyberete ve webovém nastavení. Rozhraní pokrývá Evropu, takže
  obrazovka funguje i na Slovensku nebo v Rakousku, a je tam i volba pro celou
  Evropu. Mimo Evropu se vypne stejně jako cena.

  Data jsou z ENTSO-E přes rozhraní **energy-charts**, zdarma a bez klíče.
  Zveřejňují se se zpožděním jedné až dvou hodin, proto je pod čísly napsaný
  čas vzorku. S hodinami nahoře na displeji se neshoduje a neshodovat se má.

- **Záložka Energie ve webovém nastavení.** Přirážka k burzovní ceně v Kč/MWh
  a DPH. Ukládá se okamžitě, bez restartu. Stránka pod polem rovnou přepočítá,
  kolik zadaná hodnota dělá v Kč/kWh, protože ceníky ji uvádějí právě tak.

### Změněno

- **Předpověď ukazuje víc a čte se jinak.** Bylo to šest po sobě jdoucích
  hodin a tři dny. Teď jsou nahoře tři řádky: nyní a dvě tříhodinová okna
  popsaná časem, třeba `12-15h` a `15-18h`. Okno sečte srážky, vezme nejsilnější
  vítr a nejvýraznější ikonu, takže řekne něco, co jednotlivá hodina neumí.

  Pod nimi je dnešek a dny v týdnu i s datem, tolik, kolik se jich do kruhu
  vejde. Na této desce jich vyjde sedm.

  Ovzduší se vešlo na jeden řádek. Vítr se kreslí až od 12 km/h, stejným
  pravidlem, jakým se odjakživa řídí srážky.

- **Čas se řídí polohou.** Časové pásmo bylo napevno v `Config.h`, takže deska
  odvezená do Ameriky ukazovala pražský čas a popisovala předpověď pražskými
  hodinami. Posun teď chodí spolu s předpovědí z Open-Meteo pro zadané
  souřadnice, žádný dotaz navíc to nestojí.

  V Česku zůstává zkompilované pravidlo s letním časem. Přepne se jen tehdy,
  když se s ním zjištěný posun neshoduje.

  Při té příležitosti se opravilo i to, že denní minima a maxima v předpovědi se
  počítala přes UTC dny, tedy v létě od dvou ráno do dvou ráno.

- **Na displeji je `°C` místo `degC`.** Vestavěný font ten znak má, jen se na
  něj dosud sahalo špatným kódem.

- **Sedm obrazovek místo pěti.** Obě energetické jsou po aktualizaci vypnuté,
  zapnete je ve webovém nastavení. Dokud jsou vypnuté, zařízení si o jejich
  data vůbec neřekne.

---

## [0.6.6]

### Přidáno

- **Legendu na meteoradaru jde skrýt.** Nové zaškrtávátko ve webovém nastavení,
  v sekci Meteoradar pod výběrem zdroje dat; ve výchozím stavu je legenda
  zapnutá jako dosud. Stupnice dBZ / mm/h zabírá levý okraj mapy, a kdo ji zná
  zpaměti, uvidí po vypnutí víc území. Ptalo se na to víc lidí.

  Vypnutím zmizí i pruh, který si legenda rezervovala v layoutu. Bez toho by se
  sice pixely uvolnily, ale popisky měst by se tomu místu dál vyhýbaly a
  vypnutí by nepřineslo vůbec nic.

---

## [0.6.5]

### Přidáno

- **Podpora desky Waveshare ESP32-S3-Touch-LCD-2.8C.** Jedna binárka běží na
  2.1 i na 2.8C — deska se pozná při startu, žádné dvě verze firmwaru a žádné
  přepínání před kompilací. Podstatné je, že obě desky jsou pinově naprosto
  totožné: stejných šestnáct RGB linek, stejné HSYNC/VSYNC/DE/PCLK, stejná
  dvojice SPI pinů pro příkazy ST7701, stejné podsvícení, stejná I2C, stejný
  expandér na 0x20 se stejným rozdělením EXIO i stejný pin INT dotyku. Liší se
  jen tři věci a ani jedna z nich není pin:

  1. inicializační sekvence registrů ST7701 (opravdu jiný panel — jiná gamma,
     jiné napájecí registry, celý blok E0–ED jiný),
  2. vertikální časování (VPW 3 / VBP 8 proti VPW 2 / VBP 18),
  3. dotykový řadič (CST820 na 0x15 proti GT911 na 0x5D).

  Protože nic nekoliduje elektricky, jde deska určit až za běhu. Rozhodčím je
  dotykový řadič — jediná součástka, které se dá zeptat, kdo je, a dá se jí to
  zeptat dřív, než se rozjede displej, tedy přesně ve chvíli, kdy odpověď
  potřebujeme (`Board.h` / `Board.cpp`, voláno v `setup()` mezi `TCA9554_Init()`
  a `ST7701_Init()`).

  Pixel clock je i na 2.8C 8 MHz, ačkoli výrobce u ní uvádí 18 MHz — ze stejného
  důvodu jako u 2.1 (zatížení sběrnice PSRAM). 2.8C má delší vertikální
  zatemnění, takže snímek při 8 MHz vyjde na 548 × 508 = 278 384 taktů (34,8 ms)
  proti 548 × 499 = 273 452 (34,2 ms) u 2.1 — dost blízko na to, aby ~34 ms na
  snímek, kolem kterých je postavený hlídač displeje i `FLUSH_DEBUG`, platilo na
  obou.

- **Ovladač GT911** (`Touch_GT911.*`) vedle stávajícího CST820. Tvarovaný
  záměrně stejně jako CST820: čtení řízené přerušením z INT, mlčení řadiče se
  nepovažuje za poruchu, stejné kontroly rozumnosti vzorku. Na drátě jsou to
  ale úplně jiné čipy — šestnáctibitové adresy registrů, příznak připravenosti
  bufferu, který je nutné ručně mazat, pět bodů místo jednoho. Adresu si GT911
  vybírá podle úrovně na INT při odchodu z resetu, takže je to jediné místo ve
  firmwaru, kde ESP32 tento vodič na okamžik samo budí.

- **`Touch.h`** — společné rozhraní (`TouchData`, `Touch_Init`, `Touch_Read`).
  Zbytek skice o existenci dvou řadičů neví.

- **`TOUCH_DEBUG` loguje i surová data z řadiče**, nejen rozpoznaná gesta:
  frekvence čtení, stavový registr, úroveň INT, syrové bajty bodu a hlášku,
  když se souřadnice zahodí jako mimo panel. Nejhorší případ u dotyku je ten,
  kdy řadič na I2C odpovídá bezvadně, a přesto se nic neděje — na to je výpis
  gest krátký, protože žádná nevzniknou. Surové bajty ukážou, kde se to láme.

- **Deska ve webovém stavu.** `/api/status` a stavová tabulka v prohlížeči nově
  ukazují, na jakou desku se firmware rozhodl. První věc, na kterou se ptát,
  když hlášení mluví o ujíždějícím obrazu nebo mrtvém dotyku.

- **`BOARD_FORCE` v `Config.h`** jako pojistka: `0` detekuje (výchozí), `21`
  nebo `28` vnutí desku napevno. Užitečné, když je dotykový řadič vadný —
  detekce by pak spadla na záložní desku a u té špatné to neznamená jen chybějící
  dotyk, ale rovnou ujíždějící nebo prázdný obraz.

### Opraveno

- **Zmizela hláška `GPIO isr service is not installed` při startu.**
  `detachInterrupt()` se v inicializaci dotyku volal dřív, než se vůbec nějaké
  přerušení připojilo, a GPIO driver si na to stěžoval do sériového logu.
  Odpojuje se nově jen to, co se opravdu připojilo. Týká se to i desky 2.1,
  kde ta hláška byla odjakživa.

---

## [0.6.4]

### Opraveno

- **Radar letadel zůstával prázdný.** adsb.fi začalo odpovídat chunked a
  hexadecimální hlavičky bloků zůstávaly v těle, takže se JSON rozpadl, aniž by
  to kdokoli ohlásil. Týkalo se i tras z adsb.lol.
- **Meteoradar se přestal aktualizovat.** Výpis adresáře ČHMÚ přerostl
  vyhrazených 256 kB. Nově se prohledává průběžně a na jeho velikosti nezáleží.
- **Po neúspěchu se výpis ČHMÚ stahoval každou vteřinu**, u RainVieweru se
  naopak nezkusil znovu vůbec. Oba zdroje se teď zkusí po minutě.
- **Filtr odpovědi adsb.fi mohl vyjít prázdný** a zahodit tím všechna pole.
- **Letadlo na zemi se počítalo jako v nulové výšce** (`alt_baro` nese `ground`).
- **Zařízení se mohlo restartovat během TLS handshake.** Ten má vlastní limit
  120 s, tedy šestinásobek watchdogu; nově 8 s.
- **Stažení trasy mohlo restartovat zařízení**, protože se během něj
  neresetoval watchdog.
- **Poškozený snímek meteoradaru se mohl vykreslit**, nově se kontroluje PNG
  signatura.

### Změněno

- Všechna textová stahování vedou přes `Net_GetString()`, včetně polohy podle IP
  a venkovní teploty, které měly vlastní `HTTPClient`. Nově zvládne i `http://`.
- Textové odpovědi mají strop `NET_MAX_TEXT` a čtou se do PSRAM.
- Kontrola volné paměti před TLS je na jednom místě (`Net_HeapOk()`); do teď ji
  mělo pět souborů okopírovanou.
- Velké buffery se alokují výhradně v PSRAM.
- Kromě pole `ac` se přijímá i `aircraft` (servery odvozené z ADSBexchange).
- Sériový výpis říká velikost indexu ČHMÚ a důvod nečekané odpovědi z adsb.fi.

### Odebráno

- Jednosnímkové API meteoradaru (`CHMU_FetchLatest()` a spol.) — obrazovka jede
  na animaci a tuhle cestu nevolal nikdo.
- Třídy sinku z `NetSink.h` do `.cpp`; hlavička ze 139 řádků na 54.
- Prázdné soubory `OTA.cpp` a `OTA.h`, zbytek po ElegantOTA.
- `Watchdog_Suspend()` a `Watchdog_Resume()`, konstanty `OTA_IDLE_MS`,
  `PORTAL_TIMEOUT_S`, `RV_MAX_TILES` a několik nevolaných funkcí v `Layout`,
  `UI`, `WxIcon` a `Forecast`.

---

## [0.6.3]

### Opraveno

- **V detailu letadla se ukazovala trasa, která neodpovídala skutečnosti** —
  třeba Atény → Istanbul u letadla nad Prahou. Sešly se tři příčiny:
  letadlo bez callsignu poslalo do dotazu svou ICAO adresu (`a31234` se
  normalizuje na `A31234`, což je platné číslo letu Aegean Airlines 1234),
  zdroj tras neověřoval polohu, a u letů s mezipřistáním se ukazoval první
  odlet a poslední přílet, i když letadlo letělo prostřední úsek.
- **Hlavička `User-Agent` se neposílala vůbec.** `HTTPClient::addHeader()` ji
  mlčky zahazuje, takže odcházelo výchozí `ESP32HTTPClient`. Týkalo se to
  i dotazů na adsb.fi ve starších verzích.
- **Trasa se dokreslila až s dalším stažením letadel**, tedy podle dosahu
  o 5 až 15 sekund později, než dorazila. Nově se objeví hned.
- **Keš tras si odpověď pamatovala až do restartu.** Callsigny se recyklují
  a výsledek platí k poloze, se kterou se ptalo, takže po mezipřistání
  ukazoval pořád první úsek. Záznam teď platí 20 minut (1 minutu, když
  trasa nebyla nalezena).

### Změněno

- **Trasy nově z adsb.lol** místo adsbdb.com. Spolu s callsignem se posílá
  i poloha letadla a server vrátí, jestli k ní trasa vůbec sedí — co neprojde,
  se nezobrazí. U letů s mezipřistáním se vybere ten úsek, ke kterému je
  letadlo nejblíž. Letadlo, které callsign nevysílá, se na trasu neptá vůbec.
- **Registrace a typ letadla se berou z adsb.fi** (pole `r` a `t`) ze stejné
  odpovědi, která se stahuje kvůli polohám. Odpadl tím dotaz na druhé API.

---

## [0.6.2]

### Opraveno

- Automatické střídání se po prvním doteku zastavilo a už se nerozjelo. Pauza po
  gestu byla pevných deset minut — což dávalo smysl, dokud se interval zadával
  v minutách. Nově je to trojnásobek intervalu, nejméně 30 s a nejvýš 10 minut.

### Změněno

- Aktualizace firmwaru už nepoužívá knihovnu **ElegantOTA**. Byla pod AGPL-3.0,
  což by dělalo z každé přeložené binárky AGPL dílo, i když je zdroják pod MIT.
  Stránku `/update` teď obsluhuje projekt sám nad třídou `Update` z ESP32 core.
  Ovládá se stejně, jen umí i anglicky. **Projekt je nově celý MIT.**
- Změna hesla se projeví hned, bez restartu.

---

## [0.6.1]

### Opraveno

- Obrazovka s QR kódem se překreslila datovou obrazovkou — při prvním spuštění
  i po továrním resetu. Dokud běží přístupový bod, patří displej jemu.
- V detailu letadla se místo typu ukazovalo `adsb_icao` (typ zprávy místo typu
  letounu).
- WiFi šlo změnit jen od displeje. Karta WiFi je teď na webu i po připojení.
- Změna jazyka se neukládala na záložce Ovládání.
- Nabídka nalezených měst zůstávala viset, když další hledání nic nenašlo.

### Změněno

- Automatické střídání se nastavuje v sekundách, ne v minutách (0 až 3600).
  Hodnota z dřívějších verzí se jednorázově převede.
- Většina nastavení se ukládá hned po změně. Tlačítko Uložit zůstává jen pro
  polohu, výběr obrazovek a zdroj radaru — ty vyžadují restart.
- Klepnutí už nepozastaví střídání obrazovek. Pozastaví ho jen swipe, dlouhý
  stisk nebo zásah z prohlížeče; otevřený detail letadla ho drží.

---

## [0.6.0]

Konfigurace se přesunula do prohlížeče, přibyly dvě obrazovky a meteoradar
funguje i mimo ČR.

### Přidáno

- **Webové nastavení** na `http://meteoplaneradar.local/`, dostupné trvale.
  Poloha (i vyhledáním města), obrazovky, zdroj radaru, jazyk, jas, filtry
  letadel, stavová stránka, export/import nastavení, heslo.
- **Přístupový bod vydrží, dokud nezadáte WiFi.** Vlastní captive portál,
  česky i anglicky.
- **Obrazovka hodin** — čas, datum, počasí, vteřinový prstenec. Bez Home
  Assistantu.
- **Obrazovka předpovědi** — 6 hodin a 3 dny z Open-Meteo, kvalita ovzduší a pyl.
- **RainViewer** jako alternativa k ČHMÚ — meteoradar funguje i v zahraničí.
- **Volitelné obrazovky** a jejich automatické střídání.
- **Noční režim** podle východu a západu slunce, denní a noční jas zvlášť.
- **Angličtina** na displeji, webu i v portálu.
- **Nouzové squawky** 7500/7600/7700, filtry letadel a sledovaný callsign.
- **Dálkové ovládání z webu** — přepínání obrazovek a změna rozsahu bez
  dotyku displeje.
- **Sdílený layout** — obrazovky si rezervují místo dřív, než se kreslí mapa,
  takže se popisky měst ani callsigny nepřekrývají.

### Změněno

- **OTA je na `/update`** běžné adresy zařízení. Zvláštní přístupový bod zmizel.
- **WiFiManager odstraněn** — nešel přeložit a blokoval hlavní smyčku.
- **Obrazovka Nastavení je kratší**, zbytek je v prohlížeči.
- **Dotyk reaguje i během stahování.**

### Poznámky k aktualizaci

- Nastavení z 0.5.5 zůstává, **uloženou WiFi je nutné zadat znovu** (dřív ji
  držel WiFiManager).
- Rozdělení paměti se nemění, OTA z 0.5.x funguje.
- Číslování se rozchází s forkem ok1cdj — stejné číslo neznamená stejný firmware.

---

## [0.5.5]

### Přidáno
- **Česká města mají vlastní seznam.** Evropská data obsahují jen sídla nad
  50 000 obyvatel a zkratky si generují strojově prvními čtyřmi písmeny, takže
  z Prahy byla `PRAH` a okresní města jako Znojmo úplně vypadla. Nový
  `CzCitiesData.h` proto pro Českou republiku evropská data **nahrazuje** —
  všech 59 měst i s původními zkratkami (`PHA`, `OVA`, `PLZ`) a při malých
  rozsazích plnými názvy. Dlouhá jména jsou zkrácená: `Jablonec n. N.`,
  `Zdar n. S.`, `Usti n. L.`
  Města z evropské sady, která leží v Česku, se přeskakují — párování je podle
  **souřadnic**, ne podle názvu, protože ten se v obou sadách píše jinak.
  Ověřeno proti datům: všech 21 překryvů se spáruje jedna ku jedné, nic se
  nekreslí dvakrát a Drážďany ani Žilina, které padnou do stejného obdélníku,
  o evropský záznam nepřijdou.
- **Popisky měst se už nepřekrývají.** Při rozsahu 200 km a u pohledu na celou
  republiku se jména psala přes sebe. Každý vykreslený popisek si nově zabírá
  místo a ten, který by do něj zasáhl, se zahodí celý — půlka jména pod jiným
  jménem je horší než žádné jméno. Města se kreslí po skupinách od největších,
  takže o místo přijde vždy ta menší obec vedle. Ověřeno: u pohledu na celou ČR
  53 popisků a **nula překryvů**.
- **Meteoradar kreslí stejnou evropskou mapu jako radar letadel.** Dosud měl
  vlastní obrys ČR s padesáti body; nově používá `EuMapData` s 30 894 body
  hranic a 1 100 městy, tedy stejná data i stejnou podrobnost jako letadla.
  Na rozsahu „cela CR“ jsou díky tomu vidět i obrysy sousedních států, takže
  republika nevisí v prázdnu. Soubory `CzBorder.*` a `CzBorderData.h` odpadly.
- **Čas a venkovní teplota** na jednom řádku pod tečkami výběru obrazovky, na
  radaru letadel i na meteoradaru. Dokud se čas nenačte, zůstává řádek prázdný.
  Počet letadel je nově menším písmem a na meteoradaru se indikátor snímku
  posunul níž, aby se řádek vešel.
- **Hodiny bez NTP.** Čas se bere z hlavičky `Date` v HTTP odpovědích, které
  stahujeme tak jako tak. Neprochází portem 123, takže ho poskytovatel
  internetu nemá jak zablokovat, nečeká se při startu a nepřibylo žádné
  spojení navíc.
- **Rozsah „cela CR“ na meteoradaru.** Pátý rozsah za 200 km ukazuje celou
  republiku nezávisle na tom, kde uživatel je.
- **Odkud a kam letadlo letí.** V detailu letadla přibyly dva řádky `Z:`
  a `Do:` s výchozím a cílovým městem z adsbdb.com, k tomu registrace a typ
  letounu. Rozdělení na dva řádky bylo kvůli velikosti písma — jedna řádka se
  šipkou mezi městy se musela zmenšit tak, že byla špatně čitelná. Takhle má
  každý řádek celou šířku panelu a u většiny tras vyjde největší velikost.
  Ptáme se jen na vybrané letadlo, nikdy na celý seznam, a odpověď se pamatuje.
  Řada letů trasu v databázi nemá a pak se nezobrazí nic.

### Opraveno
- **Meteoradar při načítání nezčerná.** Dosud se každých pět minut na několik
  sekund objevila černá obrazovka s nápisem „Nacitam animaci...“. Nově zůstane
  poslední platná animace na displeji a pod indikátorem snímku se objeví jen
  malá žlutá poznámka. Když stahování selže — výpadek spojení, nedostupný
  server — snímky se **nezahodí**, zůstane starší sada a v poznámce se objeví
  `bez spojeni, zobrazena starsi data`.
- **V detailu letadla chyběl popisek `Typ:`.** Přidán zpátky. Zobrazuje se
  krátký kód typu (`A320`, `B738`); adsb.fi ho hlásí jen někdy, takže když
  chybí, doplní se z adsbdb (pole `icao_type`).
- **Názvy měst s diakritikou se vykreslovaly jako změť znaků.** adsbdb vrací
  jména v Unicode — turecký İzmir se posílá jako `İzmir`, což je v UTF-8 dva
  bajty, a sedmibitový font z nich udělal dva náhodné znaky. Na displeji pak
  bylo něco jako `-?zmir`. Nově se text před vykreslením převede na ASCII:
  diakritika se zahodí, písmeno pod ní zůstane. Ověřeno na `İzmir`, `Malmö`,
  `Kraków`, `Košice`, `Gdańsk`, `Şanlıurfa` i `Tromsø`.
- **Teplota se nezobrazovala.** První pokus o stažení se počítal i tehdy, když
  ještě neběžela WiFi, a další byl pak na řadě až za deset minut. Nově se čeká
  na připojení a dokud teplota není známá, zkouší se každou minutu. Odpověď se
  navíc čte celá najednou, ne po proudu, aby ji nemohl utnout chunked přenos.
- **U velkých rozsahů skákala poloha do nesmyslů.** Výřez z obrázku ČHMÚ se
  ořezával na okraje snímku, takže přestal být vystředěný na uživatele — ale
  křížek se pořád kreslil doprostřed displeje. U Chebu na 200 km to dělalo přes
  80 km stranou, u Ostravy zhruba 10 km; z Prahy se chyba neprojevila, protože
  tam výřez na okraj nedosáhne. Nově se výřez neořezává, chybějící část se
  vykreslí černě a křížek se kreslí na skutečnou polohu.
- **Dotykový řadič se čte, jen když má co říct.** CST820 hlásí připravená data
  signálem INT, který jsme dosud vůbec nesledovali — četlo se každých pár
  milisekund bez ohledu na stav čipu. Odtud pocházela ta „vadná čtení“ a samé
  `0xFF`, které jsme od 0.5.1 zahazovali. Nově se čte na přerušení, během
  doteku průběžně, plus záchranné čtení jednou za `TOUCH_IDLE_POLL_MS`.
  Neúspěšné čtení se už nepočítá jako chyba — spící čip prostě neodpovídá.
  Vypíná se přepínačem `TOUCH_USE_INT`.

### Odebráno
- **Automatická obnova dotykového řadiče** včetně počítání vadných vzorků.
  Po opravě výše nemá co řešit, a jejím smazáním zmizelo jediné místo, které
  za běhu zapisovalo na obvod držící napájení displeje.
- Nepoužívané snímkové API ČHMÚ, `LCD_DrawBitmap` a `TCA9554_ReadOutput`.

### Změněno
- **Nové zdroje dat** — adsbdb.com (trasa letu, registrace, typ) a Open-Meteo
  (venkovní teplota), obojí zdarma a bez registrace. Do přehledu zdrojů je
  doplněna i mapa: hranice z Natural Earth (public domain) a města z GeoNames
  (CC BY 4.0), což je licence vyžadující uvedení autora — dosud chybělo.
- **Přepínání jednotek se přesunulo do Nastavení.** Je to předvolba jako každá
  jiná, ne něco, co se mění u konkrétního letadla — a v detailu letadla se tím
  uvolnilo místo pro trasu.
- **Trasa v detailu letadla je stejně velká a stejnou barvou jako letecké
  údaje nad ní.** Je to informace stejného druhu a odlišná barva naznačovala
  rozdíl, který tam není. Zmenší se jen tehdy, když se delší z obou řádků
  nevejde do panelu; případný přesah se ořízne a zakončí tečkou. Typ letounu
  a registrace nově sdílejí jeden řádek, aby na trasu zbyly dva.
- Výřezy snímků se alokují rovnou na největší rozsah, takže přepnutí rozsahu
  už nikdy nealokuje a nemá jak narazit na fragmentovanou PSRAM.

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
  letním a zimním časem. Popisek „nyni / −X min“ se počítá z pořadí snímku
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
  popisek HH:MM se odvozuje z názvu snímku, „nyni / −X min“ z jeho pořadí
  v animaci a všechna HTTPS spojení jedou přes `setInsecure()`, takže se
  neověřuje ani platnost certifikátů. Zmizelo čekání při startu i varování
  „NTP neodpovedel“. Kdyby na displeji někdy přibyly hodiny, vrátí se
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
  kterým se díváte z okna. Dřív se zadávalo „o kolik mapu otočit“, což je něco
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
  souřadnicích (4095, 4095)“, tedy jako platné klepnutí mimo panel, které
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
  jen ASCII). Například „Letadel: 12“, „Nastaveni“, „Jas“, „Poloha“,
  „Aktualizace FW“, v detailu letadla „Vyska“, „Rychlost“, „Kurz“, „Stoupani“.
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
  s posledními známými hodnotami a poznámkou „signal ztracen“.

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
  tlačítko „Firmware update“: deska vytvoří AP `MeteoPlaneRadar`, na displeji
  ukáže QR kód a firmware se nahraje z prohlížeče na `192.168.4.1/update`.
  Vyžaduje OTA rozdělení flash (`src/partitions.csv`, dva app sloty).
- **Zapamatování stavu UI** — poslední rozsah (zvlášť pro letadla a meteoradar)
  a naposledy zobrazená obrazovka se ukládají do NVS a obnoví se po restartu.
  Zápis je odložený (~2 s po poslední změně), aby swipování nezatěžovalo flash.
- **Zobrazení verze firmwaru** na obrazovce Nastavení (pod titulkem), na OTA
  obrazovce a v sériovém výpisu. Nová sdílená hlavička `MeteoPlaneRadar/Version.h`.
- **Sjednocení nastavení** do `MeteoPlaneRadar/Config.h` — časová zóna, výchozí poloha,
  rozsahy, intervaly stahování, název AP a limity na jednom místě.
- **CI build na GitHubu** — každý push se automaticky zkusí přeložit.
- Tento `CHANGELOG.md`, `.gitignore`, `sketch.yaml` a `LICENSE` v kořeni.

### Změněno
- Během OTA se na displeji ukáže jen „Probiha aktualizace…“ a **podsvícení se
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
  zmizely občasné chyby stahování „IncompleteInput“.
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
