// =============================================================================
//  MeteoPlaneRadar
//  The configuration page, as one PROGMEM string.
//
//  One file, no CDN, no build step: the device has to be configurable on a
//  network with no internet at all, so markup, style and script all ship inside
//  the firmware.
//
//  The page is split into TABS. Twenty-five controls in one column is a long
//  scroll on a phone, and most of them are set once and never touched again -
//  while the remote control and the status are opened repeatedly, so they get
//  the landing tab. Tabs rather than separate pages because every field stays
//  in the DOM: one Save button still posts the whole form, with no partial
//  saves to reconcile.
//
//  The page also translates itself. Shipping both languages as data and
//  switching them in the browser costs a few kilobytes of flash and saves a
//  second Czech string table in C that would have to be kept in step by hand.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

static const char PAGE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="cs"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="color-scheme" content="dark">
<title>MeteoPlaneRadar</title>
<style>
:root{--bg:#12161c;--card:#1b212b;--line:#2b3441;--fg:#e8edf4;--mut:#93a1b3;--acc:#37c0e8;--ok:#3fbf7f;--warn:#e8a33d;--err:#e05561}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;-webkit-text-size-adjust:100%}
header{padding:12px 16px;display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}
h1{font-size:18px;margin:0}
.ver{color:var(--mut);font-size:13px}
nav.tabs{display:flex;gap:6px;overflow-x:auto;padding:0 12px 10px;border-bottom:1px solid var(--line);
 position:sticky;top:0;background:var(--bg);z-index:5;scrollbar-width:none}
nav.tabs::-webkit-scrollbar{display:none}
nav.tabs button{background:transparent;color:var(--mut);border:1px solid var(--line);border-radius:999px;
 padding:8px 15px;white-space:nowrap;font-weight:500;font-size:14px;cursor:pointer}
nav.tabs button.on{background:var(--acc);color:#08202a;border-color:var(--acc);font-weight:600}
.wrap{max-width:820px;margin:0 auto;padding:16px 16px 96px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px;margin-bottom:14px}
.card h2{font-size:15px;margin:0 0 10px;color:var(--acc)}
.row{display:flex;align-items:center;gap:10px;margin:8px 0;flex-wrap:wrap}
.row label{flex:1 1 200px;min-width:150px}
input[type=text],input[type=password],input[type=number],select{background:#0e1218;color:var(--fg);
 border:1px solid var(--line);border-radius:6px;padding:8px 9px;min-width:110px;font-size:15px}
input[type=color]{background:#0e1218;border:1px solid var(--line);border-radius:6px;height:38px;width:60px;padding:2px}
input[type=range]{flex:1 1 160px}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--acc)}
button{background:var(--acc);color:#08202a;border:0;border-radius:7px;padding:10px 14px;font-weight:600;
 cursor:pointer;font-size:15px}
button.sec{background:#39434f;color:var(--fg)}
button.danger{background:var(--err);color:#fff}
button:disabled{cursor:default}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:8px}
.chk{display:flex;align-items:center;gap:8px}
.hint{color:var(--mut);font-size:13px;margin:8px 0 0}
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--bg);border-top:1px solid var(--line);
 padding:12px 16px calc(12px + env(safe-area-inset-bottom));display:flex;gap:12px;align-items:center;z-index:6}
#msg{font-size:14px}
.ok{color:var(--ok)}.err{color:var(--err)}.warn{color:var(--warn)}
table{width:100%;border-collapse:collapse;font-size:14px}
td{padding:5px 0;border-bottom:1px solid var(--line)}
td:first-child{color:var(--mut);width:50%}
.hide{display:none}
#scrBtns button{padding:8px 12px;font-size:14px}
@media (max-width:520px){
  .row label{flex:1 1 100%}
  .row input[type=text],.row input[type=password],.row input[type=number],.row select{flex:1 1 100%}
}
</style></head><body>
<header>
  <h1>MeteoPlaneRadar</h1><span class="ver" id="ver"></span>
  <span style="flex:1"></span>
  <select id="uiLang" onchange="setLang(this.value)"><option value="0">Čeština</option><option value="1">English</option></select>
</header>

<nav class="tabs" id="tabs">
  <button data-tab="tCtl"    class="on" data-i18n="tabCtl">Ovládání</button>
  <button data-tab="tLoc"    data-i18n="tabLoc">Poloha</button>
  <button data-tab="tScr"    data-i18n="tabScr">Obrazovky</button>
  <button data-tab="tLook"   data-i18n="tabLook">Vzhled</button>
  <button data-tab="tPlanes" data-i18n="tabPlanes">Letadla</button>
  <button data-tab="tEnergy" data-i18n="tabEnergy">Energie</button>
  <button data-tab="tHa"     data-i18n="tabHa">Home Assistant</button>
  <button data-tab="tSys"    data-i18n="tabSys">Systém</button>
</nav>

<div class="wrap">

<!-- ================= OVLÁDÁNÍ ================= -->
<section id="tCtl" class="tab">
  <div class="card hide" id="cardWifi">
    <h2 data-i18n="wifi">WiFi</h2>
    <p class="hint" id="wifiNow"></p>
    <div class="row"><label data-i18n="network">Síť</label>
      <select id="ssid" style="flex:2 1 220px"></select>
      <button class="sec" onclick="scan()" data-i18n="scan">Vyhledat</button></div>
    <div class="row"><label data-i18n="password">Heslo</label><input type="password" id="wpass" style="flex:2 1 220px"></div>
    <p class="hint" id="wifiHintTxt"></p>
    <button onclick="saveWifi()" data-i18n="connect">Připojit</button>

    <div id="wifiSlots">
      <h2 style="margin-top:18px" data-i18n="wifiSaved">Uložené sítě</h2>
      <p class="hint" data-i18n="wifiSavedHint">Zařízení si pamatuje až tři sítě.</p>
      <datalist id="ssidList"></datalist>
      <div class="row"><label>1.</label><input type="text" id="ws0" list="ssidList" maxlength="32" style="flex:2 1 160px"><input type="password" id="wp0" maxlength="64" style="flex:2 1 140px"></div>
      <div class="row"><label>2.</label><input type="text" id="ws1" list="ssidList" maxlength="32" style="flex:2 1 160px"><input type="password" id="wp1" maxlength="64" style="flex:2 1 140px"></div>
      <div class="row"><label>3.</label><input type="text" id="ws2" list="ssidList" maxlength="32" style="flex:2 1 160px"><input type="password" id="wp2" maxlength="64" style="flex:2 1 140px"></div>
      <button class="sec" onclick="saveWifiList()" data-i18n="wifiSaveList">Uložit sítě</button>
    </div>
  </div>

  <div class="card" id="cardRemote">
    <h2 data-i18n="remote">Dálkové ovládání</h2>
    <div class="row" id="scrBtns"></div>
    <div class="row">
      <button class="sec" onclick="stepScreen(-1)">&#8592;</button>
      <button class="sec" onclick="stepScreen(1)">&#8594;</button>
      <span style="flex:1"></span>
      <span data-i18n="rangeLbl">Rozsah</span>
      <button class="sec" id="rMinus" onclick="stepRange(-1)">&minus;</button>
      <b id="rangeNow" style="min-width:80px;text-align:center">&ndash;</b>
      <button class="sec" id="rPlus" onclick="stepRange(1)">+</button>
    </div>
    <p class="hint" data-i18n="remoteHint">Rozsah se mění jen na obrazovkách Letadla a Meteoradar. Zásah pozastaví automatické střídání.</p>
  </div>

  <div class="card">
    <h2 data-i18n="statusHdr">Stav zařízení</h2>
    <table id="statusTab"></table>
  </div>
</section>

<!-- ================= POLOHA ================= -->
<section id="tLoc" class="tab hide">
  <div class="card">
    <h2 data-i18n="location">Poloha</h2>
    <div class="row"><label data-i18n="findCity">Najít město</label>
      <input type="text" id="q" style="flex:2 1 200px" placeholder="Praha">
      <button class="sec" onclick="geo()" data-i18n="search">Hledat</button></div>
    <div class="row hide" id="geoRow"><label data-i18n="found">Nalezeno</label>
      <select id="geoSel" style="flex:2 1 240px" onchange="pickCity()"></select></div>
    <div class="row"><label data-i18n="lat">Zeměpisná šířka</label><input type="number" step="0.0001" id="lat"></div>
    <div class="row"><label data-i18n="lon">Zeměpisná délka</label><input type="number" step="0.0001" id="lon"></div>
    <p class="hint" data-i18n="locHint">Změna polohy vyžaduje restart, o který se zařízení postará samo.</p>
  </div>
</section>

<!-- ================= OBRAZOVKY ================= -->
<section id="tScr" class="tab hide">
  <div class="card">
    <h2 data-i18n="screens">Obrazovky</h2>
    <div class="grid">
      <label class="chk"><input type="checkbox" id="sClock"><span data-i18n="scrClock">Hodiny</span></label>
      <label class="chk"><input type="checkbox" id="sPlanes"><span data-i18n="scrPlanes">Letadla</span></label>
      <label class="chk"><input type="checkbox" id="sMeteo"><span data-i18n="scrMeteo">Meteoradar</span></label>
      <label class="chk"><input type="checkbox" id="sForecast"><span data-i18n="scrForecast">Předpověď</span></label>
      <label class="chk"><input type="checkbox" id="sPrice"><span data-i18n="scrPrice">Cena elektřiny</span></label>
      <label class="chk"><input type="checkbox" id="sMix"><span data-i18n="scrMix">Výroba ČR</span></label>
      <label class="chk"><input type="checkbox" id="sCards"><span data-i18n="scrCards">Karty</span></label>
    </div>
    <p class="hint" data-i18n="scrHint">Vypnuté obrazovky se přeskakují. Nastavení je dostupné vždy.</p>
    <p class="hint" data-i18n="restartHint">Změna obrazovek, zdroje radaru nebo polohy potřebuje restart, takže se ukládá až tlačítkem dole.</p>
    <div class="row"><label data-i18n="autoRotate">Automatické střídání (sekundy, 0 = vypnuto)</label>
      <input type="number" id="autoRotate" min="0" max="3600" step="5"></div>
    <p class="hint" data-i18n="rotHint">Střídání pozastaví přejetí prstem, dlouhý stisk nebo přepnutí z prohlížeče — na trojnásobek intervalu, pak pokračuje samo. Obyčejné klepnutí ho nezastaví, otevřený detail letadla ho drží. Na obrazovce Nastavení se nestřídá.</p>
    <div class="row"><label class="chk"><input type="checkbox" id="screenDots"><span data-i18n="scrDots">Zobrazovat tečky obrazovek nahoře</span></label></div>
    <p class="hint" data-i18n="scrDotsHint">Jedna tečka za každou obrazovku a za každou kartu. S více kartami je jich hodně, proto jsou ve výchozím stavu skryté.</p>
  </div>

  <div class="card">
    <h2 data-i18n="cardsHdr">Karty</h2>
    <p class="hint" data-i18n="cardsIntro"></p>
    <table id="cardsTab"></table>
    <div class="row" style="margin-top:12px">
      <button class="sec" onclick="document.getElementById('cardFile').click()" data-i18n="cardAdd">Přidat kartu ze souboru</button>
      <input type="file" id="cardFile" class="hide" accept="application/json,.json" onchange="cardInstall(this)">
    </div>
    <p class="hint" data-i18n="cardsHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="stormHdr">Meteoradar jen při bouřce</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="stormOnly"><span data-i18n="stormOnly">Meteoradar ve střídání jen když se blíží srážky/bouřka</span></label></div>
    <div class="row"><label data-i18n="stormDbz">Práh odrazivosti (dBZ)</label><input type="number" id="stormDbz" min="4" max="60" step="1"></div>
    <div class="row"><label data-i18n="stormKm">Vzdálenost od zařízení (km)</label><input type="number" id="stormKm" min="5" max="250" step="5"></div>
    <p class="hint" data-i18n="stormHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="paHdr">Letadla jen když je letadlo poblíž</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="planeAlert"><span data-i18n="planeAlert">Ukázat letadla jen když je letadlo blízko</span></label></div>
    <div class="row"><label data-i18n="paKm">Vzdálenost (km)</label><input type="number" id="planeAlertKm" min="1" max="50" step="1"></div>
    <div class="row"><label data-i18n="paSec">Jak dlouho ukázat (sekundy)</label><input type="number" id="planeAlertSec" min="5" max="300" step="5"></div>
    <p class="hint" data-i18n="paHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="radar">Meteoradar</h2>
    <div class="row"><label data-i18n="radarSrc">Zdroj dat</label>
      <select id="radarSrc">
        <option value="0" data-i18n="srcChmu">ČHMÚ (ostřejší, jen ČR)</option>
        <option value="2" data-i18n="srcChmuM">ČHMÚ masked (jen srážky dopadající na zem)</option>
        <option value="1" data-i18n="srcRv">RainViewer (Evropa i svět)</option>
      </select></div>
    <p class="hint" data-i18n="srcDiff"></p>
    <p class="hint" data-i18n="radarHint">Mimo ČR nemá ČHMÚ data a obrazovka zůstane prázdná — použijte RainViewer.</p>
    <div class="row"><label class="chk"><input type="checkbox" id="meteoLegend"><span data-i18n="mtLegend">Zobrazit legendu (dBZ / mm/h)</span></label></div>
    <p class="hint" data-i18n="mtLegendHint">Legenda zabírá levý okraj mapy. Když stupnici znáte, dá se skrýt a je vidět víc území.</p>
  </div>
</section>

<!-- ================= VZHLED ================= -->
<section id="tLook" class="tab hide">
  <div class="card">
    <h2 data-i18n="brightness">Jas</h2>
    <div class="row"><label data-i18n="briDay">Denní jas</label><input type="range" id="briDay" min="10" max="100"><span id="briDayV"></span></div>
    <div class="row"><label data-i18n="briNight">Noční jas</label><input type="range" id="briNight" min="5" max="100"><span id="briNightV"></span></div>
    <div class="row"><label class="chk"><input type="checkbox" id="nightAuto"><span data-i18n="nightAuto">Přepínat automaticky podle slunce</span></label></div>
    <div class="row"><label data-i18n="nightOffset">Posun proti východu/západu (min)</label><input type="number" id="nightOffset" min="-120" max="120"></div>
    <p class="hint" data-i18n="liveHint">Změny na této záložce se ukládají hned, tlačítko Uložit tu nepotřebujete.</p>
  </div>

  <div class="card">
    <h2 data-i18n="clockHdr">Hodiny</h2>
    <div class="row"><label data-i18n="secStyle">Vteřinový prstenec</label>
      <select id="secStyle">
        <option value="0" data-i18n="secOff">vypnuto</option>
        <option value="1" data-i18n="secDots">tečky</option>
        <option value="2" data-i18n="secLine">čára</option>
        <option value="3" data-i18n="secComet">kometa</option>
      </select></div>
    <div class="row"><label data-i18n="clockFont">Písmo (všechny obrazovky)</label>
      <select id="clockFont">
        <option value="1">Inter</option>
        <option value="2">Poppins</option>
        <option value="3" data-i18n="fontThin">Inter tenké</option>
        <option value="0" data-i18n="fontPixel">pixelové (původní)</option>
      </select></div>
    <div class="row"><label data-i18n="clockColor">Barva hodin</label><input type="color" id="clockColor"></div>
    <div class="row"><label data-i18n="secColor">Barva vteřin</label><input type="color" id="secColor"></div>
  </div>
</section>

<!-- ================= LETADLA ================= -->
<section id="tPlanes" class="tab hide">
  <div class="card">
    <h2 data-i18n="planes">Letadla</h2>
    <div class="row"><label data-i18n="altMin">Nejnižší výška (ft)</label><input type="number" id="altMin" min="0" max="60000"></div>
    <div class="row"><label data-i18n="altMax">Nejvyšší výška (ft)</label><input type="number" id="altMax" min="0" max="60000"></div>
    <div class="row"><label class="chk"><input type="checkbox" id="onlyCallsign"><span data-i18n="onlyCs">Jen letadla s callsignem</span></label></div>
    <div class="row"><label class="chk"><input type="checkbox" id="squawkAlert"><span data-i18n="sqAlert">Upozornit na nouzové squawky (7500/7600/7700)</span></label></div>
    <div class="row"><label data-i18n="watch">Sledovaný callsign nebo ICAO</label><input type="text" id="watch" maxlength="9" placeholder="CSA1234"></div>
    <p class="hint" data-i18n="planesHint">Filtry se týkají jen kreslení. Nouzový squawk ani sledované letadlo neschovají.</p>
    <p class="hint" data-i18n="liveHint">Změny na této záložce se ukládají hned, tlačítko Uložit tu nepotřebujete.</p>
  </div>

  <div class="card">
    <h2 data-i18n="planesView">Zobrazení radaru</h2>
    <div class="row"><label data-i18n="topBearing">Nahoře na radaru</label>
      <select id="topBearing"><option value="0">S</option><option value="45">SV</option><option value="90">V</option><option value="135">JV</option><option value="180">J</option><option value="225">JZ</option><option value="270">Z</option><option value="315">SZ</option></select></div>
    <div class="row"><label class="chk"><input type="checkbox" id="metric"><span data-i18n="metric">Metrické jednotky (m, km/h)</span></label></div>
    <p class="hint" data-i18n="planesViewHint">Nastavte směr, kterým se díváte z okna. Meteoradar se záměrně neotáčí. Jednotky platí pro detail letadla.</p>
  </div>

  <div class="card">
    <h2 data-i18n="altHdr">Barvy podle výšky</h2>
    <div class="row"><label data-i18n="altBelow">Pod 1. hladinou</label><input type="color" id="ac0"></div>
    <div class="row"><label data-i18n="altB1">1. hladina (m)</label><input type="number" id="ab0" min="100" max="20000" step="100"><input type="color" id="ac1"></div>
    <div class="row"><label data-i18n="altB2">2. hladina (m)</label><input type="number" id="ab1" min="100" max="20000" step="100"><input type="color" id="ac2"></div>
    <div class="row"><label data-i18n="altB3">3. hladina (m)</label><input type="number" id="ab2" min="100" max="20000" step="100"><input type="color" id="ac3"></div>
    <div class="row"><button class="sec" onclick="altDefaults()" data-i18n="altDef">Výchozí barvy a hladiny</button></div>
    <p class="hint" data-i18n="altHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="winHdr">Výhled z okna</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="windowOn"><span data-i18n="winOn">Používat výhled z okna</span></label></div>
    <div class="row"><label data-i18n="winAz">Směr okna</label>
      <select id="windowAz">
        <option value="0" data-i18n="dir0">sever (S)</option>
        <option value="45" data-i18n="dir45">severovýchod (SV)</option>
        <option value="90" data-i18n="dir90">východ (V)</option>
        <option value="135" data-i18n="dir135">jihovýchod (JV)</option>
        <option value="180" data-i18n="dir180">jih (J)</option>
        <option value="225" data-i18n="dir225">jihozápad (JZ)</option>
        <option value="270" data-i18n="dir270">západ (Z)</option>
        <option value="315" data-i18n="dir315">severozápad (SZ)</option>
      </select></div>
    <div class="row"><label data-i18n="winW">Šířka výhledu (°)</label><input type="number" id="windowWidth" min="10" max="300" step="5"></div>
    <div class="row"><label data-i18n="winKm">Dohled (km)</label><input type="number" id="windowKm" min="1" max="100" step="1"></div>
    <p class="hint" data-i18n="winHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="iconHdr">Ikony letadel</h2>
    <p class="hint" data-i18n="iconIntro"></p>
    <div id="iconLegend" style="display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:6px 14px;margin-top:10px"></div>
    <p class="hint" data-i18n="iconCredit"></p>
  </div>
</section>

<!-- ================= ENERGIE ================= -->
<section id="tEnergy" class="tab hide">
  <div class="card">
    <h2 data-i18n="energyPrice">Cena elektřiny</h2>
    <p class="hint warn" data-i18n="priceCzOnly">Ceny platí jen pro Česko a jsou v korunách. Když nastavíte polohu mimo ČR, obrazovka se sama vypne a zařízení si o data přestane říkat.</p>
    <p class="hint" data-i18n="priceIntro">Obrazovka ukazuje burzovní (spotovou) cenu z denního trhu OTE jako dvacetičtyřhodinový ciferník. Ceny na zítřek se zveřejňují kolem 14:00, do té doby se dá zobrazit jen dnešek.</p>
    <div class="row"><label data-i18n="priceFee">Přirážka k burzovní ceně (Kč/MWh)</label>
      <input type="number" id="priceFee" min="0" max="20000" step="10"></div>
    <p class="hint" id="feeKwh"></p>
    <div class="row"><label data-i18n="priceVat">DPH (%, 0 = nepřipočítávat)</label>
      <input type="number" id="priceVat" min="0" max="100" step="1"></div>
    <p class="hint" data-i18n="priceFeeHint">Ve výchozím stavu jsou obě pole nula a displej ukazuje čistou burzovní cenu — a napíše k ní „burza“, aby nemohla být zaměněna s tím, co platíte. Distribuci, regulované složky a obchodníkovu marži si opište z vyúčtování; ceník je obvykle uvádí v Kč/kWh, takže je vynásobte tisícem. Zařízení si nic nedomýšlí: připočte přesně to, co sem zadáte, a nic jiného.</p>
  </div>

  <div class="card">
    <h2 data-i18n="energyMix">Výroba ČR</h2>
    <div class="row"><label data-i18n="mixCountry">Země</label>
      <select id="mixCountry">
        <option value="eu">Celá Evropa</option>
        <option value="cz">Česko</option>
        <option value="sk">Slovensko</option>
        <option value="at">Rakousko</option>
        <option value="de">Německo</option>
        <option value="pl">Polsko</option>
        <option value="hu">Maďarsko</option>
        <option value="si">Slovinsko</option>
        <option value="hr">Chorvatsko</option>
        <option value="be">Belgie</option>
        <option value="bg">Bulharsko</option>
        <option value="ch">Švýcarsko</option>
        <option value="dk">Dánsko</option>
        <option value="ee">Estonsko</option>
        <option value="es">Španělsko</option>
        <option value="fi">Finsko</option>
        <option value="fr">Francie</option>
        <option value="gr">Řecko</option>
        <option value="ie">Irsko</option>
        <option value="it">Itálie</option>
        <option value="lt">Litva</option>
        <option value="lu">Lucembursko</option>
        <option value="lv">Lotyšsko</option>
        <option value="nl">Nizozemsko</option>
        <option value="no">Norsko</option>
        <option value="pt">Portugalsko</option>
        <option value="ro">Rumunsko</option>
        <option value="rs">Srbsko</option>
        <option value="se">Švédsko</option>
        <option value="ua">Ukrajina</option>
        <option value="uk">Velká Británie</option>
      </select></div>
    <p class="hint" data-i18n="mixCountryHint">Rozhraní energy-charts pokrývá Evropu. Mimo ni obrazovka data nemá a sama se vypne.</p>
    <p class="hint" data-i18n="mixHint">Prstenec ukazuje, z čeho se právě vyrábí elektřina, uprostřed je podíl obnovitelných zdrojů a dole saldo přeshraničních toků. Data pocházejí z ENTSO-E přes rozhraní energy-charts a zveřejňují se se zpožděním jedné až dvou hodin — čas vzorku je proto napsaný nahoře na obrazovce.</p>
    <p class="hint" data-i18n="energySrcHint">Oba zdroje jsou zdarma, bez klíče a bez registrace. Když jsou obě obrazovky vypnuté, zařízení si o data vůbec neřekne.</p>
  </div>
</section>

<!-- ================= HOME ASSISTANT ================= -->
<section id="tHa" class="tab hide">
  <div class="card">
    <h2>Home Assistant</h2>
    <p class="hint warn" data-i18n="haUntested"></p>
    <div class="row"><label class="chk"><input type="checkbox" id="haEnabled"><span data-i18n="haOn">Zpřístupnit data pro Home Assistant</span></label></div>
    <p class="hint" data-i18n="haHow"></p>
    <p><b id="haUrl"></b> &nbsp;<a id="haLink" target="_blank" rel="noopener" data-i18n="haOpen">otevřít</a></p>
    <p class="hint" data-i18n="haYamlHint"></p>
    <pre id="haYaml" style="background:#0e1218;border:1px solid var(--line);border-radius:6px;padding:10px;overflow-x:auto;font-size:12px;white-space:pre"></pre>
    <button class="sec" onclick="copyHa()" data-i18n="haCopy">Zkopírovat</button>
  </div>
</section>

<!-- ================= SYSTÉM ================= -->
<section id="tSys" class="tab hide">
  <div class="card">
    <h2 data-i18n="system">Systém</h2>
    <div class="row"><label data-i18n="adminPass">Současné heslo</label>
      <input type="password" id="adminPass" placeholder="•••"></div>
    <div class="row"><label data-i18n="newPass">Nové heslo</label>
      <input type="password" id="newPass" placeholder="•••"></div>
    <p class="hint" id="pwState"></p>
    <p class="hint" data-i18n="passHint">Současné heslo je potřeba pro aktualizaci, import, reset i pro změnu hesla. Prázdné nové heslo nic nemění; jedna mezera ochranu zruší.</p>
    <p class="hint" data-i18n="passNote">Poznámka: výchozí stav je bez hesla — aktualizace, import i reset jsou pak přístupné komukoli v síti. Heslo se ukládá v čitelné podobě, protože aktualizační stránka používá HTTP Basic, kde otisk použít nelze; chrání tedy před domácností, ne před útočníkem. Zapomenuté heslo jde zrušit jedině podržením tlačítka BOOT při startu (~3 s), což smaže i WiFi a všechna ostatní nastavení.</p>
    <div class="row" style="margin-top:12px">
      <button class="sec" onclick="location='/update'" data-i18n="fwUpdate">Aktualizace firmwaru</button>
      <button class="sec" onclick="exportCfg()" data-i18n="export">Export nastavení</button>
      <button class="sec" onclick="document.getElementById('imp').click()" data-i18n="import">Import</button>
      <input type="file" id="imp" class="hide" accept="application/json" onchange="importCfg(this)">
      <button class="sec" onclick="doReboot()" data-i18n="reboot">Restart</button>
      <button class="danger" onclick="doReset()" data-i18n="factory">Tovární reset</button>
    </div>

    <!-- Data sources. This is not decoration: GeoNames, CHMI, RainViewer,
         Open-Meteo and Energy-Charts all require their source to be credited,
         and RainViewer asks for a link back. The 480x480 panel has no room for
         a legible credit line, so this panel is where the project discharges
         that obligation. Do not delete it in a fork - replace it. -->
    <h2 style="margin-top:22px" data-i18n="srcTitle">Zdroje dat</h2>
    <p class="hint" data-i18n="srcIntro">Data pocházejí z těchto služeb. Několik z nich vyžaduje uvedení zdroje, proto jsou uvedeny zde. Podrobnosti a podmínky jsou v souboru LICENSE.txt.</p>
    <p class="hint">
      <a href="https://adsb.fi" target="_blank" rel="noopener">adsb.fi</a> ·
      <a href="https://adsb.lol" target="_blank" rel="noopener">adsb.lol</a> ·
      <a href="https://opendata.chmi.cz" target="_blank" rel="noopener">ČHMÚ</a> ·
      <a href="https://www.rainviewer.com" target="_blank" rel="noopener">RainViewer</a> ·
      <a href="https://open-meteo.com" target="_blank" rel="noopener">Open-Meteo</a> ·
      <a href="https://spotovaelektrina.cz" target="_blank" rel="noopener">spotovaelektrina.cz</a> (OTE) ·
      <a href="https://energy-charts.info" target="_blank" rel="noopener">Energy-Charts.info</a> (Fraunhofer ISE, CC BY 4.0) ·
      <a href="https://www.geonames.org" target="_blank" rel="noopener">GeoNames</a> (CC BY 4.0) ·
      Natural Earth ·
      <a href="http://ip-api.com" target="_blank" rel="noopener">ip-api.com</a> ·
      <span data-i18n="srcIcons">ikony letadel podle</span> <a href="https://adsb-radar.com" target="_blank" rel="noopener">ADS-B Radar</a>
    </p>
  </div>
</section>

</div>

<div class="bar hide" id="saveBar"><button onclick="save()" data-i18n="save">Uložit nastavení</button><span id="msg"></span></div>

<script>
const D={
 cs:{tabCtl:"Ovládání",tabLoc:"Poloha",tabScr:"Obrazovky",tabLook:"Vzhled",tabPlanes:"Letadla",tabEnergy:"Energie",tabSys:"Systém",
  scrPrice:"Cena elektřiny",scrMix:"Výroba ČR",
  scrCards:"Karty",cardsHdr:"Karty",cardAdd:"Přidat kartu ze souboru",cardDel:"Odebrat",
  cardsNone:"Zatím žádná karta.",cardBad:"Soubor není platný JSON",cardErr:"bez dat",cardWait:"čeká",
  cardsIntro:"Karta je malý soubor JSON, který říká, odkud se má stáhnout hodnota a jak ji ukázat — třeba kurzy ČNB. Neobsahuje žádný kód. Každá karta je samostatná obrazovka a střídá se s ostatními ve stejném intervalu.",
  cardsHint:"Karta způsobí, že si zařízení pravidelně stahuje adresu, kterou v ní určil její autor — přidávejte jen karty, kterým rozumíte. Je-li nastavené heslo správce, vyplňte ho nejdřív na kartě Systém. Formát a ukázky jsou ve složce cards v repozitáři projektu.",
  energyPrice:"Cena elektřiny",energyMix:"Výroba ČR",
  mixCountry:"Země",
  mixCountryHint:"Rozhraní energy-charts pokrývá Evropu. Mimo ni obrazovka data nemá a sama se vypne.",
  priceCzOnly:"Ceny platí jen pro Česko a jsou v korunách. Když nastavíte polohu mimo ČR, obrazovka se sama vypne a zařízení si o data přestane říkat.",
  timezone:"Časové pásmo",
  priceIntro:"Obrazovka ukazuje burzovní (spotovou) cenu z denního trhu OTE jako dvacetičtyřhodinový ciferník. Ceny na zítřek se zveřejňují kolem 14:00, do té doby se dá zobrazit jen dnešek.",
  priceFee:"Přirážka k burzovní ceně (Kč/MWh)",priceVat:"DPH (%, 0 = nepřipočítávat)",
  priceFeeHint:"Ve výchozím stavu jsou obě pole nula a displej ukazuje čistou burzovní cenu — a napíše k ní „burza“, aby nemohla být zaměněna s tím, co platíte. Distribuci, regulované složky a obchodníkovu marži si opište z vyúčtování; ceník je obvykle uvádí v Kč/kWh, takže je vynásobte tisícem. Zařízení si nic nedomýšlí: připočte přesně to, co sem zadáte, a nic jiného.",
  feeKwh:"Odpovídá",
  mixHint:"Prstenec ukazuje, z čeho se právě vyrábí elektřina, uprostřed je podíl obnovitelných zdrojů a dole saldo přeshraničních toků. Data pocházejí z ENTSO-E přes rozhraní energy-charts a zveřejňují se se zpožděním jedné až dvou hodin — čas vzorku je proto napsaný nahoře na obrazovce.",
  energySrcHint:"Oba zdroje jsou zdarma, bez klíče a bez registrace. Když jsou obě obrazovky vypnuté, zařízení si o data vůbec neřekne.",
  wifi:"WiFi",network:"Síť",password:"Heslo",scan:"Vyhledat",connect:"Připojit",
  wifiHint:"Po uložení se zařízení připojí a přístupový bod zmizí.",
  wifiHintSta:"Změna sítě přeruší spojení s touto stránkou. Když se připojení nepovede, zařízení vytvoří vlastní síť MeteoPlaneRadar a zadáte ji znovu.",
  wifiNow:"Připojeno k síti",
  autoSaved:"Uloženo",
  liveHint:"Změny na této záložce se ukládají hned, tlačítko Uložit tu nepotřebujete.",
  restartHint:"Změna obrazovek, zdroje radaru nebo polohy potřebuje restart, takže se ukládá až tlačítkem dole.",
  remote:"Dálkové ovládání",rangeLbl:"Rozsah",statusHdr:"Stav zařízení",
  remoteHint:"Rozsah se mění jen na obrazovkách Letadla a Meteoradar. Zásah pozastaví automatické střídání.",
  location:"Poloha",findCity:"Najít město",search:"Hledat",found:"Nalezeno",lat:"Zeměpisná šířka",lon:"Zeměpisná délka",
  locHint:"Změna polohy vyžaduje restart, o který se zařízení postará samo.",
  screens:"Obrazovky",scrClock:"Hodiny",scrPlanes:"Letadla",scrMeteo:"Meteoradar",scrForecast:"Předpověď",
  board:"Deska",
  scrHint:"Vypnuté obrazovky se přeskakují. Nastavení je dostupné vždy.",autoRotate:"Automatické střídání (sekundy, 0 = vypnuto)",
  rotHint:"Střídání pozastaví přejetí prstem, dlouhý stisk nebo přepnutí z prohlížeče — na trojnásobek intervalu, pak pokračuje samo. Obyčejné klepnutí ho nezastaví, otevřený detail letadla ho drží. Na obrazovce Nastavení se nestřídá.",
  radar:"Meteoradar",radarSrc:"Zdroj dat",srcChmu:"ČHMÚ (ostřejší, jen ČR)",srcRv:"RainViewer (Evropa i svět)",
  mtLegend:"Zobrazit legendu (dBZ / mm/h)",
  scrDots:"Zobrazovat tečky obrazovek nahoře",
  scrDotsHint:"Jedna tečka za každou obrazovku a za každou kartu. S více kartami je jich hodně, proto jsou ve výchozím stavu skryté.",
  mtLegendHint:"Legenda zabírá levý okraj mapy. Když stupnici znáte, dá se skrýt a je vidět víc území.",
  radarHint:"Mimo ČR nemá ČHMÚ data a obrazovka zůstane prázdná — použijte RainViewer.",
  brightness:"Jas",clockHdr:"Hodiny",
  briDay:"Denní jas",briNight:"Noční jas",nightAuto:"Přepínat automaticky podle slunce",
  nightOffset:"Posun proti východu/západu (min)",secStyle:"Vteřinový prstenec",secOff:"vypnuto",secDots:"tečky",secLine:"čára",secComet:"kometa",
  clockColor:"Barva hodin",secColor:"Barva vteřin",metric:"Metrické jednotky (m, km/h)",topBearing:"Nahoře na radaru letadel",
  planes:"Letadla",altMin:"Nejnižší výška (ft)",altMax:"Nejvyšší výška (ft)",onlyCs:"Jen letadla s callsignem",
  sqAlert:"Upozornit na nouzové squawky (7500/7600/7700)",watch:"Sledovaný callsign nebo ICAO",
  planesHint:"Filtry se týkají jen kreslení. Nouzový squawk ani sledované letadlo neschovají.",
  planesView:"Zobrazení radaru",
  planesViewHint:"Nastavte směr, kterým se díváte z okna. Meteoradar se záměrně neotáčí. Jednotky platí pro detail letadla.",
  pwNone:"Zatím není nastavené žádné heslo — aktualizace, import a reset jsou otevřené.",
  pwSet:"Heslo je nastavené.",
  passNote:"Poznámka: výchozí stav je bez hesla — aktualizace, import i reset jsou pak přístupné komukoli v síti. Heslo se ukládá v čitelné podobě, protože aktualizační stránka používá HTTP Basic, kde otisk použít nelze; chrání tedy před domácností, ne před útočníkem. Zapomenuté heslo jde zrušit jedině podržením tlačítka BOOT při startu (~3 s), což smaže i WiFi a všechna ostatní nastavení.",
  srcTitle:"Zdroje dat",
  srcIntro:"Data pocházejí z těchto služeb. Několik z nich vyžaduje uvedení zdroje, proto jsou uvedeny zde. Podrobnosti a podmínky jsou v souboru LICENSE.txt.",
  system:"Systém",adminPass:"Současné heslo",newPass:"Nové heslo",
  passHint:"Současné heslo je potřeba pro aktualizaci, import, reset i pro změnu hesla. Prázdné nové heslo nic nemění; jedna mezera ochranu zruší.",
  fwUpdate:"Aktualizace firmwaru",export:"Export nastavení",import:"Import",reboot:"Restart",factory:"Tovární reset",save:"Uložit nastavení",
  settings:"Nastavení",
  saved:"Uloženo",failed:"Nepovedlo se",searching:"Hledám…",nothing:"Nic nenalezeno",
  disabled:"Obrazovka je vypnutá",confirmReset:"Opravdu smazat všechna nastavení včetně WiFi?",
  tabHa:"Home Assistant",wifiSaved:"Uložené sítě",
  wifiSavedHint:"Zařízení si pamatuje až tři sítě. Lze je zapsat ručně i když zrovna nejsou v dosahu — třeba síť v kanceláři ještě doma. Po výpadku spojení zkusí zařízení po minutě ostatní uložené sítě, nejsilnější jako první. Prázdné pole hesla nechá uložené heslo beze změny. Uložení sítí zařízení neodpojí.",
  wifiSaveList:"Uložit sítě",passStored:"heslo uloženo",passNone:"heslo",ssidPh:"název sítě",
  stormHdr:"Meteoradar jen při bouřce",stormOnly:"Meteoradar ve střídání jen když se blíží srážky/bouřka",
  stormDbz:"Práh odrazivosti (dBZ)",stormKm:"Vzdálenost od zařízení (km)",
  stormHint:"Každých 5 minut se zkontroluje nejnovější snímek radaru ČHMÚ. Když je v zadané vzdálenosti odraz aspoň zadané síly, meteoradar se přidá do střídání; po dvou klidných snímcích zase vypadne. Funguje i když je obrazovka Meteoradar výše vypnutá. Orientačně: 20 dBZ slabý déšť, 40 dBZ silný déšť, nad 50 dBZ bouřka s možným krupobitím. Data jsou jen z ČHMÚ, takže v ČR a kousek za hranicemi. Potřebuje zapnuté automatické střídání.",
  paHdr:"Letadla jen když je letadlo poblíž",planeAlert:"Ukázat letadla jen když je letadlo blízko (nebo ve výhledu z okna)",
  paKm:"Vzdálenost (km)",paSec:"Jak dlouho ukázat (sekundy)",
  paHint:"Obrazovka Letadla se pak sama neobjevuje ve střídání. Jakmile se letadlo přiblíží na zadanou vzdálenost, zařízení na ni přepne, otevře detail toho letadla a po zadané době se vrátí tam, kde bylo. Každé letadlo se ukáže jen jednou (za 15 minut). Funguje i bez automatického střídání. Výhled z okna se nastavuje na záložce Letadla a má přednost — letadlo, které z okna vidíte, se ukáže, i když je dál než jiné za domem. Dotyk během ukázky ji ukončí a zůstane se na letadlech.",
  srcChmuM:"ČHMÚ masked (jen srážky dopadající na zem)",
  srcDiff:"ČHMÚ ukazuje všechno, co radary vidí, plnou barvou — včetně srážek, které se vypaří dřív, než dopadnou, a kovadlin vzdálených bouřek. ČHMÚ masked je tentýž snímek, ale plnou barvou jen tam, kde podle měření ve výšce 2 km srážky opravdu padají na zem; zbytek je bledší. Pro otázku „bude u nás pršet?“ je masked přesnější, pro sledování bouřek je lepší plný.",
  clockFont:"Písmo (všechny obrazovky)",fontThin:"Inter tenké",fontPixel:"pixelové (původní)",
  altHdr:"Barvy podle výšky",altBelow:"Pod 1. hladinou",altB1:"1. hladina (m)",altB2:"2. hladina (m)",altB3:"3. hladina (m)",
  altDef:"Výchozí barvy a hladiny",
  altHint:"Barva u hladiny platí od té výšky nahoru až k další hladině. Výchozí je červená pod 2 km, oranžová 2–6 km, žlutá 6–10 km a modrá nad 10 km. Hladiny musí jít vzestupně. Legenda na displeji se přizpůsobí.",
  altBad:"Hladiny musí jít vzestupně",
  winHdr:"Výhled z okna",winOn:"Používat výhled z okna",winAz:"Směr okna",
  dir0:"sever (S)",dir45:"severovýchod (SV)",dir90:"východ (V)",dir135:"jihovýchod (JV)",
  dir180:"jih (J)",dir225:"jihozápad (JZ)",dir270:"západ (Z)",dir315:"severozápad (SZ)",winW:"Šířka výhledu (°)",winKm:"Dohled (km)",
  winHint:"Výseč oblohy, kterou z okna vidíte: kam okno míří, jak široký úhel oblohy z něj vidíte (výseč je rozložená rovnoměrně kolem směru okna — 90° u okna na SV pokryje sever až východ) a jak daleko letadlo ještě rozeznáte. Na radaru letadel se zakreslí tenkými čarami. Letadlo ve výhledu má při upozornění přednost před bližším letadlem, které z okna vidět není.",
  haUntested:"Tato funkce zatím nebyla vyzkoušena se skutečným Home Assistantem. Formát odpovídá dokumentaci integrace RESTful, ale pokud něco nesedí, dejte vědět.",
  haOn:"Zpřístupnit data pro Home Assistant",
  haHow:"Home Assistant si data sám stahuje (zhruba jednou za minutu) z adresy níže. Nic se nikam neposílá a nepotřebujete MQTT. Data jsou jen ke čtení: teplota, vítr, srážky, bouřka v okolí, nejbližší letadlo, cena elektřiny. Dokud je to zapnuté, zařízení hlídá i bouřku (jeden snímek ČHMÚ za 5 minut) a letadla poblíž (malý dotaz každých 30 s).",
  haOpen:"otevřít",haYamlHint:"Do souboru configuration.yaml v Home Assistantu vložte tohle a Home Assistant restartujte:",haCopy:"Zkopírovat",copied:"Zkopírováno",
  stormSt:"Bouřka",planeWatchSt:"Letadla poblíž",
  srcIcons:"ikony letadel podle",iconHdr:"Ikony letadel",
  iconIntro:"Tvar ikony na radaru podle typu letadla, velikost odpovídá hmotnosti. Typ se pozná z kódu typu (A20N, B738, EC35…), u neznámého z kategorie, kterou vysílá transpondér. Barva ikony udává výšku (viz níže).",
  iconCredit:"Rozdělení typů letadel do ikon vychází ze schématu ADS-B Radar (adsb-radar.com/help/icons.html). Tvary kreslí firmware sám, soubory ikon se nepřebírají.",
  icNarrow:"úzkotrupé — A320, B737, B757",icWide:"širokotrupé dvoumotorové — A330, A350, B767, B777, B787",
  icQuad:"čtyřmotorové — A340, A380, B747",icRegional:"regionální — CRJ, Embraer E-Jet, Fokker 100",
  icBizjet:"business jet — Citation, Learjet, Falcon, Gulfstream",icTwinprop:"turbovrtulové — ATR, Dash 8, King Air, L 410",
  icLight:"malá letadla — Cessna, Piper, Zlin, ultralighty",icMiltrans:"vojenská transportní — C-130, C-160, A400M",
  icFreighter:"těžká nákladní — An-124, C-17, C-5, Il-76",icFighter:"stíhačky a cvičné proudové",
  icGlider:"kluzáky a motorové kluzáky",icTilt:"konvertoplány — V-22, AW609",icHeli:"vrtulníky",
  icBalloon:"balony a vzducholodě",icDrone:"drony",icGeneric:"nezjištěný typ",icNoTrack:"neznámý směr letu",},
 en:{tabCtl:"Control",tabLoc:"Location",tabScr:"Screens",tabLook:"Appearance",tabPlanes:"Aircraft",tabEnergy:"Energy",tabSys:"System",
  scrPrice:"Electricity price",scrMix:"Czech generation",
  scrCards:"Cards",cardsHdr:"Cards",cardAdd:"Add a card from a file",cardDel:"Remove",
  cardsNone:"No card yet.",cardBad:"The file is not valid JSON",cardErr:"no data",cardWait:"waiting",
  cardsIntro:"A card is a small JSON file that says where to fetch a value and how to show it - the CNB exchange rates, for example. It contains no code. Every card is a screen of its own and takes its turn with the others at the same interval.",
  cardsHint:"A card makes the device fetch, at intervals, an address chosen by whoever wrote it - only add cards you understand. If an admin password is set, fill it in on the System tab first. The format and samples are in the cards folder of the project repository.",
  energyPrice:"Electricity price",energyMix:"Czech generation",
  mixCountry:"Country",
  mixCountryHint:"The energy-charts API covers Europe. Outside it the screen has no data and switches itself off.",
  priceCzOnly:"Prices are the Czech market, in CZK. Set a location outside Czechia and the screen switches itself off and the device stops asking for the data.",
  timezone:"Time zone",
  priceIntro:"The screen shows the day-ahead exchange (spot) price from the OTE market as a 24-hour dial. Tomorrow's prices are published around 14:00 local time; until then only today can be shown.",
  priceFee:"Surcharge on the exchange price (CZK/MWh)",priceVat:"VAT (%, 0 = do not add)",
  priceFeeHint:"Both fields default to zero, so the display shows the bare exchange price and labels it \"spot\" - it can never be mistaken for what you actually pay. Copy distribution, the regulated components and your supplier's margin off your bill; tariffs usually quote them per kWh, so multiply by a thousand. The device invents nothing: it adds exactly what you enter here and nothing else.",
  feeKwh:"That is",
  mixHint:"The ring shows what electricity is being generated from right now, the renewable share sits in the middle and the cross-border balance at the foot. The data is ENTSO-E's, served by the energy-charts API, and is published one to two hours behind - which is why the sample time is printed at the top of the screen.",
  energySrcHint:"Both sources are free, without a key and without registration. With both screens switched off the device never asks for the data at all.",
  wifi:"WiFi",network:"Network",password:"Password",scan:"Scan",connect:"Connect",
  wifiHint:"After saving the device connects and the access point disappears.",
  wifiHintSta:"Changing the network drops this page. If the connection fails, the device brings up its own MeteoPlaneRadar network and you enter it again.",
  wifiNow:"Connected to",
  autoSaved:"Saved",
  liveHint:"Changes on this tab are saved immediately - no need for the Save button.",
  restartHint:"Changing the screens, the radar source or the location needs a restart, so those are saved with the button below.",
  remote:"Remote control",rangeLbl:"Range",statusHdr:"Device status",
  remoteHint:"The range only applies to the Aircraft and Weather screens. Using this pauses the automatic cycling.",
  location:"Location",findCity:"Find a town",search:"Search",found:"Found",lat:"Latitude",lon:"Longitude",
  locHint:"Changing the location needs a restart, which the device does by itself.",
  screens:"Screens",scrClock:"Clock",scrPlanes:"Aircraft",scrMeteo:"Weather radar",scrForecast:"Forecast",
  board:"Board",
  scrHint:"Disabled screens are skipped. Settings is always reachable.",autoRotate:"Auto cycling (seconds, 0 = off)",
  rotHint:"Cycling is paused by a swipe, a long press or a switch from the browser - for three times the interval, then it resumes on its own. A plain tap does not stop it; an open aircraft detail holds it. It does not run on the Settings screen.",
  radar:"Weather radar",radarSrc:"Data source",srcChmu:"CHMU (sharper, Czechia only)",srcRv:"RainViewer (Europe and beyond)",
  mtLegend:"Show the legend (dBZ / mm/h)",
  scrDots:"Show the screen dots at the top",
  scrDotsHint:"One dot per screen and one per card. With several cards there are many of them, so they are hidden by default.",
  mtLegendHint:"The legend takes up the left edge of the map. If you know the scale, hide it and see more ground.",
  radarHint:"Outside Czechia CHMU has no data and the screen stays blank — use RainViewer.",
  brightness:"Brightness",clockHdr:"Clock",
  briDay:"Day brightness",briNight:"Night brightness",nightAuto:"Switch automatically with the sun",
  nightOffset:"Offset from sunrise/sunset (min)",secStyle:"Seconds ring",secOff:"off",secDots:"dots",secLine:"line",secComet:"comet",
  clockColor:"Clock colour",secColor:"Seconds colour",metric:"Metric units (m, km/h)",topBearing:"Top of the aircraft radar",
  planes:"Aircraft",altMin:"Lowest altitude (ft)",altMax:"Highest altitude (ft)",onlyCs:"Only aircraft with a callsign",
  sqAlert:"Alert on emergency squawks (7500/7600/7700)",watch:"Watched callsign or ICAO",
  planesHint:"Filters affect drawing only. They never hide an emergency squawk or a watched aircraft.",
  planesView:"Radar view",
  planesViewHint:"Set the direction you are looking out of the window. The weather radar deliberately does not turn. Units apply to the aircraft detail.",
  pwNone:"No password is set yet - update, import and reset are open.",
  pwSet:"A password is set.",
  passNote:"Note: the default is no password, which leaves update, import and reset open to anyone on the network. The password is stored in the clear because the update page uses HTTP Basic, where a digest cannot be used - so it protects against the household, not against an attacker. A forgotten password can only be cleared by holding BOOT at startup (~3 s), which also erases WiFi and every other setting.",
  srcTitle:"Data sources",
  srcIntro:"The data comes from the services below. Several of them require their source to be credited, which is why they are listed here. The full terms are in LICENSE.txt.",
  system:"System",adminPass:"Current password",newPass:"New password",
  passHint:"The current password is needed for updates, import, reset and to change the password itself. An empty new password changes nothing; a single space removes the protection.",
  fwUpdate:"Firmware update",export:"Export settings",import:"Import",reboot:"Reboot",factory:"Factory reset",save:"Save settings",
  settings:"Settings",
  saved:"Saved",failed:"Failed",searching:"Searching…",nothing:"Nothing found",
  disabled:"That screen is switched off",confirmReset:"Really erase all settings including WiFi?",
  tabHa:"Home Assistant",wifiSaved:"Saved networks",
  wifiSavedHint:"The device remembers up to three networks. They can be typed in even when out of range - set up the office network while still at home. When the link drops, after a minute the device tries the other saved networks, strongest first. An empty password field keeps the stored password. Saving does not disconnect the device.",
  wifiSaveList:"Save networks",passStored:"password stored",passNone:"password",ssidPh:"network name",
  stormHdr:"Weather radar only for storms",stormOnly:"Weather radar in the cycling only when rain/a storm is coming",
  stormDbz:"Reflectivity threshold (dBZ)",stormKm:"Distance from the device (km)",
  stormHint:"Every 5 minutes the newest CHMU radar frame is checked. When an echo at least this strong lies within this distance, the weather radar joins the cycling; after two clear frames it drops out again. Works even with the Weather radar screen switched off above. Roughly: 20 dBZ light rain, 40 dBZ heavy rain, above 50 dBZ a storm with possible hail. The data is CHMU's, so Czechia and a little beyond. Needs automatic cycling on.",
  paHdr:"Aircraft only when one is near",planeAlert:"Show the aircraft only when one is near (or in the window view)",
  paKm:"Distance (km)",paSec:"How long to show it (seconds)",
  paHint:"The Aircraft screen then never comes up in the cycling by itself. As soon as an aircraft comes this close, the device switches to it, opens that aircraft's detail and after the set time goes back to where it was. Each aircraft is shown once (per 15 minutes). Works without automatic cycling too. The window view is set on the Aircraft tab and takes priority - an aircraft you can see from the window is shown even if it is further away than another one behind the house. Touching the screen during the alert ends it and stays on the aircraft.",
  srcChmuM:"CHMU masked (only rain reaching the ground)",
  srcDiff:"CHMU shows everything the radars see at full colour - including rain that evaporates before it reaches the ground and the anvils of distant storms. CHMU masked is the same picture, but at full colour only where measurements at 2 km say the rain actually reaches the ground; the rest is paler. For 'will it rain here?' masked is more accurate, for watching storms the full one is better.",
  clockFont:"Typeface (all screens)",fontThin:"Inter thin",fontPixel:"pixel (original)",
  altHdr:"Colours by altitude",altBelow:"Below level 1",altB1:"Level 1 (m)",altB2:"Level 2 (m)",altB3:"Level 3 (m)",
  altDef:"Default colours and levels",
  altHint:"A level's colour applies from that altitude up to the next level. The default is red below 2 km, orange 2-6 km, yellow 6-10 km and blue above 10 km. Levels must ascend. The legend on the display follows.",
  altBad:"The levels must ascend",
  winHdr:"Window view",winOn:"Use the window view",winAz:"Window faces",
  dir0:"north (N)",dir45:"north-east (NE)",dir90:"east (E)",dir135:"south-east (SE)",
  dir180:"south (S)",dir225:"south-west (SW)",dir270:"west (W)",dir315:"north-west (NW)",winW:"Width of the view (°)",winKm:"Visibility (km)",
  winHint:"The slice of sky you see from the window: which way the window faces, how wide an angle of sky you see from it (spread evenly around that direction - 90° on a north-east window covers north to east) and how far you can still make out an aircraft. It is drawn as thin lines on the aircraft radar. An aircraft in the view takes priority for the alert over a nearer one you cannot see.",
  haUntested:"This feature has not been tested with a real Home Assistant yet. The format follows the RESTful integration's documentation, but if something does not fit, please report it.",
  haOn:"Make the data available to Home Assistant",
  haHow:"Home Assistant fetches the data itself (about once a minute) from the address below. Nothing is sent anywhere and no MQTT is needed. The data is read-only: temperature, wind, rain, storm nearby, nearest aircraft, electricity price. While this is on, the device also watches for storms (one CHMU frame every 5 minutes) and nearby aircraft (a small request every 30 s).",
  haOpen:"open",haYamlHint:"Paste this into Home Assistant's configuration.yaml and restart Home Assistant:",haCopy:"Copy",copied:"Copied",
  stormSt:"Storm",planeWatchSt:"Aircraft nearby",
  srcIcons:"aircraft icons after",iconHdr:"Aircraft icons",
  iconIntro:"The icon's shape follows the aircraft type and its size the weight. The type comes from the type code (A20N, B738, EC35...), or from the category the transponder sends when the type is unknown. The colour shows the altitude (see below).",
  iconCredit:"The assignment of aircraft types to icons follows the scheme of ADS-B Radar (adsb-radar.com/help/icons.html). The shapes are drawn by the firmware itself; no icon files are used.",
  icNarrow:"narrow-body - A320, B737, B757",icWide:"wide-body twin - A330, A350, B767, B777, B787",
  icQuad:"four-engine - A340, A380, B747",icRegional:"regional jets - CRJ, Embraer E-Jet, Fokker 100",
  icBizjet:"business jets - Citation, Learjet, Falcon, Gulfstream",icTwinprop:"turboprops - ATR, Dash 8, King Air, L 410",
  icLight:"light aircraft - Cessna, Piper, Zlin, ultralights",icMiltrans:"military transport - C-130, C-160, A400M",
  icFreighter:"heavy freighters - An-124, C-17, C-5, Il-76",icFighter:"fighters and jet trainers",
  icGlider:"gliders and motor gliders",icTilt:"tiltrotors - V-22, AW609",icHeli:"helicopters",
  icBalloon:"balloons and airships",icDrone:"drones",icGeneric:"type not identified",icNoTrack:"direction of flight unknown",}};
let L="cs", CFG={}, TAB="tCtl";
const $=id=>document.getElementById(id);

function setLang(v){L=(v==1||v=="en")?"en":"cs";$("uiLang").value=(L=="en")?"1":"0";
 document.documentElement.lang=L;
 document.querySelectorAll("[data-i18n]").forEach(e=>{const t=D[L][e.dataset.i18n];if(t)e.textContent=t;});
 pwState();feeHint();status();if(CFG.wifi)wifiSlots();iconLegend();}

// Tabs. The Save bar is hidden on Control - nothing there is saved, it is sent
// straight away - so the button cannot look like it applies to the buttons.
function showTab(id){
 TAB=id;
 document.querySelectorAll("section.tab").forEach(s=>s.classList.toggle("hide",s.id!=id));
 document.querySelectorAll("#tabs button").forEach(b=>b.classList.toggle("on",b.dataset.tab==id));
 $("saveBar").classList.toggle("hide",id=="tCtl");
 window.scrollTo(0,0);
}
document.querySelectorAll("#tabs button").forEach(b=>b.onclick=()=>showTab(b.dataset.tab));

function msg(t,c){$("msg").textContent=t;$("msg").className=c||"";setTimeout(()=>{$("msg").textContent=""},4000);}
function rgb565ToHex(v){const r=(v>>11&31)*255/31|0,g=(v>>5&63)*255/63|0,b=(v&31)*255/31|0;
 return "#"+[r,g,b].map(x=>x.toString(16).padStart(2,"0")).join("");}
function hexToRgb565(h){const r=parseInt(h.substr(1,2),16),g=parseInt(h.substr(3,2),16),b=parseInt(h.substr(5,2),16);
 return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}

const SCR=[["scrClock",0],["scrPlanes",1],["scrMeteo",2],["scrForecast",3],
           ["scrPrice",4],["scrMix",5],["scrCards",6],["settings",7]];
function drawScrBtns(cur,enabled){
 $("scrBtns").innerHTML=SCR.map(([k,i])=>{
  const on=enabled?enabled[i]:true;
  const cls=(i==cur)?"":"sec";
  const dis=on?"":" disabled style='opacity:.4'";
  return "<button class='"+cls+"'"+dis+" onclick='goScreen("+i+")'>"+D[L][k]+"</button>";
 }).join(" ");
}
async function post(u,b){try{const r=await fetch(u,{method:"POST",headers:{"Content-Type":"application/json"},
 body:JSON.stringify(b)});
 if(r.status==409)msg(D[L].disabled,"warn");else if(!r.ok)msg(D[L].failed,"err");
 await status();}catch(e){msg(D[L].failed,"err")}}
function goScreen(i){post("/api/screen",{index:i});}
function stepScreen(d){post("/api/screen",{step:d});}
function stepRange(d){post("/api/range",{step:d});}

// --- Okamzite ukladani ------------------------------------------------------
// The API applies only the keys it actually receives, so a single field can be
// sent on its own without disturbing anything else.
//
// Deliberately NOT every field: the location, the set of screens and the radar
// source make the device restart, and auto-saving those while someone is still
// typing coordinates would reboot it mid-keystroke. Those keep the Save button.
let saveTimer = {};
function autoSave(key, val){
 clearTimeout(saveTimer[key]);
 saveTimer[key] = setTimeout(async () => {
  const o = {}; o[key] = val;
  try{
   const r = await fetch("/api/config",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(o)});
   msg(r.ok ? D[L].autoSaved : D[L].failed, r.ok ? "ok" : "err");
  }catch(e){ msg(D[L].failed,"err"); }
 }, 350);
}

// [id, event, JSON key, how to read the value]
const AUTO = [
 ["uiLang","change","lang",e=>+e.value],
 ["metric","change","metric",e=>e.checked],
 ["topBearing","change","topBearing",e=>+e.value],
 ["briDay","input","briDay",e=>+e.value],
 ["briNight","input","briNight",e=>+e.value],
 ["nightAuto","change","nightAuto",e=>e.checked],
 ["nightOffset","change","nightOffset",e=>+e.value],
 ["secStyle","change","secStyle",e=>+e.value],
 ["clockColor","change","clockColor",e=>hexToRgb565(e.value)],
 ["secColor","change","secColor",e=>hexToRgb565(e.value)],
 ["altMin","change","altMin",e=>+e.value],
 ["altMax","change","altMax",e=>+e.value],
 ["onlyCallsign","change","onlyCallsign",e=>e.checked],
 ["squawkAlert","change","squawkAlert",e=>e.checked],
 ["meteoLegend","change","meteoLegend",e=>e.checked],
 ["screenDots","change","screenDots",e=>e.checked],
 ["watch","change","watch",e=>e.value],
 ["autoRotate","change","autoRotate",e=>+e.value],
 // The tariff fields change nothing but a multiplication, so they take effect
 // on the next redraw and do not need the Save button or a restart.
 ["priceFee","change","priceFee",e=>+e.value],
 ["priceVat","change","priceVat",e=>+e.value],
 ["mixCountry","change","mixCountry",e=>e.value],
 ["clockFont","change","clockFont",e=>+e.value],
 ["stormOnly","change","stormOnly",e=>e.checked],
 ["stormDbz","change","stormDbz",e=>+e.value],
 ["stormKm","change","stormKm",e=>+e.value],
 ["planeAlert","change","planeAlert",e=>e.checked],
 ["planeAlertKm","change","planeAlertKm",e=>+e.value],
 ["planeAlertSec","change","planeAlertSec",e=>+e.value],
 ["windowOn","change","windowOn",e=>e.checked],
 ["windowAz","change","windowAz",e=>+e.value],
 ["windowWidth","change","windowWidth",e=>+e.value],
 ["windowKm","change","windowKm",e=>+e.value],
 ["haEnabled","change","haEnabled",e=>e.checked],
];
// The altitude levels and the colours each go as one set - applied one value
// at a time, the levels would pass through an out-of-order state and be refused.
["ab0","ab1","ab2"].forEach(id=>$(id).addEventListener("change",()=>altSave("bands")));
["ac0","ac1","ac2","ac3"].forEach(id=>$(id).addEventListener("change",()=>altSave("colors")));
function altBands(){return[+$("ab0").value,+$("ab1").value,+$("ab2").value];}
function altColors(){return[0,1,2,3].map(i=>hexToRgb565($("ac"+i).value));}
function altOk(b){return b[0]>0&&b[0]<b[1]&&b[1]<b[2];}
function altSave(what){
 if(what=="bands"){const b=altBands();if(!altOk(b)){msg(D[L].altBad,"warn");return;}autoSave("altBands",b);}
 else autoSave("altColors",altColors());
}
function altDefaults(){
 [2000,6000,10000].forEach((v,i)=>$("ab"+i).value=v);
 [0xF800,0xFC00,0xFFE0,0x001F].forEach((v,i)=>$("ac"+i).value=rgb565ToHex(v));
 autoSave("altBands",altBands());autoSave("altColors",altColors());
}
function wireAutoSave(){
 AUTO.forEach(([id,ev,key,get])=>{
  const el=$(id); if(!el) return;
  el.addEventListener(ev,()=>autoSave(key,get(el)));
 });
}

async function load(){
 const r=await fetch("/api/config");CFG=await r.json();
 setLang(CFG.lang);
 $("ver").textContent="v"+CFG.version;
 // The WiFi card is shown in BOTH modes. Hiding it once connected meant that
 // swapping a router forced you back to the touchscreen, or to a factory reset.
 $("cardWifi").classList.remove("hide");
 if(CFG.apMode){$("cardRemote").classList.add("hide");scan();}
 $("wifiHintTxt").textContent=CFG.apMode?D[L].wifiHint:D[L].wifiHintSta;
 $("lat").value=CFG.lat.toFixed(4);$("lon").value=CFG.lon.toFixed(4);
 $("sClock").checked=CFG.screens.clock;$("sPlanes").checked=CFG.screens.planes;
 $("sMeteo").checked=CFG.screens.meteo;$("sForecast").checked=CFG.screens.forecast;
 $("sPrice").checked=!!CFG.screens.price;$("sMix").checked=!!CFG.screens.mix;
 $("sCards").checked=!!CFG.screens.cards;cardsLoad();
 $("priceFee").value=CFG.priceFee||0;$("priceVat").value=CFG.priceVat||0;
 $("mixCountry").value=CFG.mixCountry||"cz";
 feeHint();
 $("autoRotate").value=CFG.autoRotate;$("radarSrc").value=CFG.radarSrc;
 $("meteoLegend").checked=CFG.meteoLegend;
 $("screenDots").checked=!!CFG.screenDots;
 $("briDay").value=CFG.briDay;$("briNight").value=CFG.briNight;
 $("nightAuto").checked=CFG.nightAuto;$("nightOffset").value=CFG.nightOffset;
 $("secStyle").value=CFG.secStyle;$("metric").checked=CFG.metric;$("topBearing").value=CFG.topBearing;
 $("clockColor").value=rgb565ToHex(CFG.clockColor);$("secColor").value=rgb565ToHex(CFG.secColor);
 $("altMin").value=CFG.altMin;$("altMax").value=CFG.altMax;
 pwState();
 $("onlyCallsign").checked=CFG.onlyCallsign;$("squawkAlert").checked=CFG.squawkAlert;$("watch").value=CFG.watch||"";
 $("clockFont").value=(CFG.clockFont!=null)?CFG.clockFont:1;
 (CFG.altBands||[2000,6000,10000]).forEach((v,i)=>$("ab"+i).value=v);
 (CFG.altColors||[0xF800,0xFC00,0xFFE0,0x001F]).forEach((v,i)=>$("ac"+i).value=rgb565ToHex(v));
 $("stormOnly").checked=!!CFG.stormOnly;$("stormDbz").value=CFG.stormDbz||20;$("stormKm").value=CFG.stormKm||50;
 $("planeAlert").checked=!!CFG.planeAlert;$("planeAlertKm").value=CFG.planeAlertKm||5;$("planeAlertSec").value=CFG.planeAlertSec||15;
 $("windowOn").checked=!!CFG.windowOn;// Eight directions; an older value between them snaps to the nearest one.
 $("windowAz").value=String((Math.round(((CFG.windowAz!=null)?CFG.windowAz:180)/45)%8)*45);
 $("windowWidth").value=CFG.windowWidth||90;$("windowKm").value=CFG.windowKm||15;
 $("haEnabled").checked=!!CFG.haEnabled;haInfo();
 wifiSlots();
 if(CFG.apMode)$("wifiSlots").classList.add("hide");
 bri();wireAutoSave();status();
}
// Says whether a password is actually set - more use than a static sentence,
// and it is the first thing to check when /update stops asking for one.
function pwState(){
 if(!$("pwState"))return;
 const on=!!CFG.hasPassword;
 $("pwState").textContent=on?D[L].pwSet:D[L].pwNone;
 $("pwState").className="hint "+(on?"ok":"warn");
}
function bri(){$("briDayV").textContent=$("briDay").value+"%";$("briNightV").textContent=$("briNight").value+"%";}
$("briDay").addEventListener("input",bri);$("briNight").addEventListener("input",bri);

// Suppliers quote the surcharge per kWh and the field takes it per MWh, which
// is exactly the kind of thousandfold mistake nobody notices until the display
// says 1800 Kc/kWh. Echoing the value back in the unit the user copied it from
// makes a slipped decimal point obvious while they are still looking at it.
function feeHint(){
 const f=+$("priceFee").value||0, v=+$("priceVat").value||0;
 if(!f&&!v){$("feeKwh").textContent="";return;}
 const kwh=f*(1+v/100)/1000;
 const num=kwh.toFixed(2).replace(".",L=="cs"?",":".");
 const unit=(L=="cs")?" Kč/kWh":" CZK/kWh";
 const vatNote=v?((L=="cs")?" včetně DPH":" incl. VAT"):"";
 $("feeKwh").textContent=D[L].feeKwh+" "+num+unit+vatNote;
}
$("priceFee").addEventListener("input",feeHint);
$("priceVat").addEventListener("input",feeHint);

async function status(){
 try{const s=await(await fetch("/api/status")).json();
 drawScrBtns(s.screen,s.enabled);
 if(!CFG.apMode&&$("wifiNow")) $("wifiNow").textContent=D[L].wifiNow+" "+s.ssid+".";
 const hasR=(s.range&&s.range.length>0);
 $("rangeNow").textContent=hasR?s.range:"–";
 $("rMinus").disabled=!hasR;$("rPlus").disabled=!hasR;
 $("rMinus").style.opacity=$("rPlus").style.opacity=hasR?"1":".4";
 const rows=[["IP",s.ip],["WiFi",s.ssid+" ("+s.rssi+" dBm)"],["Uptime",s.uptime],
  ["Heap",s.heap+" B"],["PSRAM",s.psram+" B"],["Restart",s.resetReason],
  ["ADS-B",s.adsb],["Radar",s.radar],[D[L].scrForecast,s.forecast],
  [D[L].scrPrice,s.price],[D[L].scrMix,s.mix],[D[L].scrCards,s.cards],
  [D[L].stormSt,s.storm],[D[L].planeWatchSt,s.planeWatch],
  [D[L].timezone,s.timezone],
  ["Firmware","v"+s.version],[D[L].board,s.board]];
 $("statusTab").innerHTML=rows.map(r=>"<tr><td>"+r[0]+"</td><td>"+r[1]+"</td></tr>").join("");}catch(e){}
}
setInterval(status,10000);

function body(){return{lat:parseFloat($("lat").value),lon:parseFloat($("lon").value),
 lang:parseInt($("uiLang").value),metric:$("metric").checked,
 briDay:+$("briDay").value,briNight:+$("briNight").value,nightAuto:$("nightAuto").checked,
 nightOffset:+$("nightOffset").value,radarSrc:+$("radarSrc").value,autoRotate:+$("autoRotate").value,
 meteoLegend:$("meteoLegend").checked,
 screenDots:$("screenDots").checked,
 topBearing:+$("topBearing").value,secStyle:+$("secStyle").value,
 clockColor:hexToRgb565($("clockColor").value),secColor:hexToRgb565($("secColor").value),
 altMin:+$("altMin").value,altMax:+$("altMax").value,onlyCallsign:$("onlyCallsign").checked,
 squawkAlert:$("squawkAlert").checked,watch:$("watch").value,
 password:$("adminPass").value,newPassword:$("newPass").value,
 priceFee:+$("priceFee").value,priceVat:+$("priceVat").value,
 mixCountry:$("mixCountry").value,
 clockFont:+$("clockFont").value,altColors:altColors(),
 stormOnly:$("stormOnly").checked,stormDbz:+$("stormDbz").value,stormKm:+$("stormKm").value,
 planeAlert:$("planeAlert").checked,planeAlertKm:+$("planeAlertKm").value,planeAlertSec:+$("planeAlertSec").value,
 windowOn:$("windowOn").checked,windowAz:+$("windowAz").value,windowWidth:+$("windowWidth").value,windowKm:+$("windowKm").value,
 haEnabled:$("haEnabled").checked,
 screens:{clock:$("sClock").checked,planes:$("sPlanes").checked,meteo:$("sMeteo").checked,
  forecast:$("sForecast").checked,price:$("sPrice").checked,mix:$("sMix").checked,
  cards:$("sCards").checked}};}

async function save(){
 const bb=body(),ab=altBands();if(altOk(ab))bb.altBands=ab;
 const r=await fetch("/api/config",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(bb)});
 if(r.ok){msg(D[L].saved,"ok");$("newPass").value="";setLang($("uiLang").value);}else msg(D[L].failed+" ("+r.status+")","err");
}
async function scan(){
 const s=$("ssid");s.innerHTML="<option>…</option>";
 try{const l=await(await fetch("/api/scan")).json();
 s.innerHTML=l.map(n=>"<option value='"+n.ssid+"'>"+n.ssid+" ("+n.rssi+")</option>").join("");
 $("ssidList").innerHTML=l.map(n=>"<option value='"+n.ssid+"'>").join("");}catch(e){s.innerHTML=""}
}
// The three remembered networks. Passwords never come back from the device,
// so an empty field means "keep what is stored" and the placeholder says
// whether there is anything stored.
function wifiSlots(){
 (CFG.wifi||[]).forEach((w,i)=>{if(!$("ws"+i))return;$("ws"+i).value=w.ssid||"";$("wp"+i).value="";
  $("ws"+i).placeholder=D[L].ssidPh;$("wp"+i).placeholder=w.hasPass?D[L].passStored:D[L].passNone;});
}
async function saveWifiList(){
 const nets=[0,1,2].map(i=>({ssid:$("ws"+i).value.trim(),pass:$("wp"+i).value}));
 try{const r=await fetch("/api/wifilist",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({nets})});
 msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");
 if(r.ok){const c=await(await fetch("/api/config")).json();CFG.wifi=c.wifi;wifiSlots();}}catch(e){msg(D[L].failed,"err")}
}
// Home Assistant: the address to poll and a ready configuration.yaml block.
function haInfo(){
 const host=(CFG.ip&&CFG.ip!="0.0.0.0")?CFG.ip:"meteoplaneradar.local";
 const url="http://"+host+"/api/ha";
 $("haUrl").textContent=url;$("haLink").href=url;
 const q='"';
 const S=(n,t,u,dc,extra)=>"      - name: "+q+n+q+"\n        unique_id: "+n.toLowerCase().replace(/[^a-z0-9]+/g,"_")+
  "\n        value_template: "+q+"{{ "+t+" }}"+q+"\n"+(u?"        unit_of_measurement: "+q+u+q+"\n":"")+
  (dc?"        device_class: "+dc+"\n":"")+(extra||"");
 // Weather is null until the first forecast lands - unavailable, not zero.
 const av=k=>"        availability: "+q+"{{ value_json."+k+" is not none }}"+q+"\n";
 $("haYaml").textContent=
  "rest:\n  - resource: "+url+"\n    scan_interval: 60\n    sensor:\n"+
  S("MPR Teplota","value_json.temperature","°C","temperature","        state_class: measurement\n"+av("temperature"))+
  S("MPR Vitr","value_json.wind_kmh","km/h","wind_speed","        state_class: measurement\n"+av("wind_kmh"))+
  S("MPR Srazky","value_json.precip_mm","mm","precipitation",av("precip_mm"))+
  S("MPR Bourka max dBZ","value_json.storm.max_dbz | default(0)","dBZ")+
  S("MPR Bourka vzdalenost","value_json.storm.nearest_km","km","distance",
   "        availability: "+q+"{{ value_json.storm.nearest_km is defined and value_json.storm.nearest_km is not none }}"+q+"\n")+
  S("MPR Letadla v okoli","value_json.aircraft.count")+
  S("MPR Nejblizsi letadlo","value_json.aircraft.callsign | default('-', true)",null,null,
   "        json_attributes_path: "+q+"$.aircraft"+q+"\n        json_attributes:\n"+
   ["distance_km","altitude_m","speed_kmh","type","kind","bearing","in_window"].map(a=>"          - "+a+"\n").join(""))+
  S("MPR Cena elektriny","value_json.price_czk_kwh","CZK/kWh",null,
   "        availability: "+q+"{{ value_json.price_czk_kwh is defined }}"+q+"\n")+
  "    binary_sensor:\n"+
  "      - name: "+q+"MPR Bourka"+q+"\n        unique_id: mpr_bourka\n        value_template: "+q+"{{ 'on' if value_json.storm.active else 'off' }}"+q+"\n        device_class: safety\n"+
  "      - name: "+q+"MPR Letadlo nablizku"+q+"\n        unique_id: mpr_letadlo_nablizku\n        value_template: "+q+"{{ 'on' if value_json.aircraft.alert else 'off' }}"+q+"\n";
}
// --- Aircraft icon legend ----------------------------------------------------
// The same polygon tables PlaneIcon.cpp draws on the display, so the legend
// cannot drift from what the radar actually shows. A part is
// [count, mirror, x0,y0, x1,y1, ...] with y pointing forward (nose up).
const SH={"GENERIC":[10,0,0,12,3,1,13,-8,3,-5,3,-7,0,-12,-3,-7,-3,-5,-13,-8,-3,1,0],"NARROW":[7,0,0,14,2,11,2,-10,1,-14,-1,-14,-2,-10,-2,11,4,1,2,4,13,-3,13,-5,2,-1,4,1,4,1,6,0,6,-3,4,-3,4,1,1,-9,6,-12,6,-14,1,-12,0],"WIDE":[7,0,0,14,3,10,3,-10,1,-14,-1,-14,-3,-10,-3,10,4,1,3,4,15,-4,15,-6,3,-1,4,1,5,2,8,1,8,-3,5,-3,4,1,1,-9,7,-12,7,-14,1,-12,0],"QUAD":[7,0,0,14,3,10,3,-10,1,-14,-1,-14,-3,-10,-3,10,4,1,3,4,15,-4,15,-6,3,-1,4,1,5,2,7,1,7,-2,5,-2,4,1,9,-1,11,-2,11,-5,9,-5,4,1,1,-9,7,-12,7,-14,1,-12,0],"REGIONAL":[7,0,0,13,2,10,2,-9,1,-13,-1,-13,-2,-9,-2,10,4,1,2,3,11,-2,11,-4,2,-1,4,1,2,-5,4,-5,4,-9,2,-9,4,1,0,-11,6,-12,6,-13,0,-13,0],"BIZJET":[7,0,0,12,2,8,2,-8,1,-12,-1,-12,-2,-8,-2,8,4,1,2,2,10,-4,10,-5,2,-1,4,1,2,-5,4,-5,4,-8,2,-8,4,1,0,-10,5,-12,5,-13,0,-12,0],"TWINPROP":[6,0,0,13,2,10,2,-11,0,-13,-2,-11,-2,10,4,1,2,3,14,2,14,-1,2,-1,4,1,4,6,6,6,6,-1,4,-1,4,1,1,-9,6,-10,6,-12,1,-12,0],"LIGHT":[6,0,0,10,2,8,2,-9,0,-11,-2,-9,-2,8,4,1,2,5,14,5,14,2,2,2,4,1,1,-8,5,-8,5,-10,1,-10,0],"MILTRANS":[6,0,0,13,3,10,3,-10,0,-13,-3,-10,-3,10,4,1,3,4,16,3,16,0,3,0,4,1,5,7,7,7,7,0,5,0,4,1,10,6,12,6,12,0,10,0,4,1,1,-9,7,-10,7,-12,1,-12,0],"FREIGHTER":[7,0,0,14,3,10,3,-10,1,-14,-1,-14,-3,-10,-3,10,4,1,3,5,16,-1,16,-3,3,1,4,1,6,3,8,2,8,-1,6,-1,4,1,11,1,13,0,13,-3,11,-3,4,1,0,-11,7,-13,7,-14,0,-14,0],"FIGHTER":[6,0,0,13,2,8,2,-11,1,-13,-1,-13,-2,-11,3,1,1,6,10,-6,1,-6,3,1,1,-8,5,-12,1,-12,0],"GLIDER":[6,0,0,10,1,8,1,-10,0,-11,-1,-10,-1,8,4,1,1,3,17,2,17,1,1,1,4,1,0,-9,4,-9,4,-10,0,-10,0],"TILT":[6,0,0,12,2,9,2,-10,0,-12,-2,-10,-2,9,4,1,2,2,10,2,10,-1,2,-1,4,1,1,-8,5,-9,5,-11,1,-11,0]};
const ICONS=[ // [shape, scale, key] - order and sizes as on the display
 ["NARROW",1,"icNarrow"],["WIDE",1.25,"icWide"],["QUAD",1.4,"icQuad"],["REGIONAL",.95,"icRegional"],
 ["BIZJET",.85,"icBizjet"],["TWINPROP",.9,"icTwinprop"],["LIGHT",.8,"icLight"],["MILTRANS",1.15,"icMiltrans"],
 ["FREIGHTER",1.4,"icFreighter"],["FIGHTER",.9,"icFighter"],["GLIDER",.85,"icGlider"],["TILT",1,"icTilt"],
 ["HELI",1,"icHeli"],["BALLOON",1,"icBalloon"],["DRONE",1,"icDrone"],["GENERIC",1,"icGeneric"],["UNKNOWN",1,"icNoTrack"]];
function iconSvg(name,k){
 const c="#e8edf4";let b="";
 if(name=="HELI")b="<circle r='11' fill='none' stroke='"+c+"' stroke-width='1.3'/><circle r='4' fill='"+c+"'/><path d='M0 0V13M-3 13H3' stroke='"+c+"' stroke-width='2'/>";
 else if(name=="BALLOON")b="<circle cy='-3' r='7' fill='"+c+"'/><path d='M-3 3L-2 7M3 3L2 7' stroke='"+c+"'/><rect x='-2' y='7' width='5' height='3' fill='"+c+"'/>";
 else if(name=="DRONE")b="<path d='M-7-7L7 7M-7 7L7-7' stroke='"+c+"' stroke-width='1.3'/>"+[[-7,-7],[7,-7],[-7,7],[7,7]].map(p=>"<circle cx='"+p[0]+"' cy='"+p[1]+"' r='3' fill='none' stroke='"+c+"'/>").join("")+"<rect x='-2' y='-2' width='5' height='5' fill='"+c+"'/>";
 else if(name=="UNKNOWN")b="<circle r='7' fill='none' stroke='"+c+"' stroke-width='1.3'/><circle r='2' fill='"+c+"'/>";
 else{const a=SH[name];let i=0;while(a[i]){const n=a[i],m=a[i+1],v=a.slice(i+2,i+2+2*n);
  for(const sg of (m?[1,-1]:[1]))b+="<polygon fill='"+c+"' points='"+v.map((x,j)=>j%2?(-x*k).toFixed(1):(sg*x*k).toFixed(1)).reduce((o,x,j)=>o+(j%2?","+x+" ":x),"")+"'/>";
  i+=2+2*n;}}
 return "<svg viewBox='-21 -21 42 42' width='42' height='42' style='flex:0 0 42px'>"+b+"</svg>";
}
function iconLegend(){
 $("iconLegend").innerHTML=ICONS.map(([n,k,key])=>"<div class='chk' style='gap:10px'>"+iconSvg(n,k)+
  "<span style='font-size:14px'>"+D[L][key]+"</span></div>").join("");
}
function copyHa(){
 const t=$("haYaml").textContent;
 if(navigator.clipboard&&window.isSecureContext){navigator.clipboard.writeText(t).then(()=>msg(D[L].copied,"ok"));return;}
 // Plain http on the LAN is not a secure context, where the clipboard API is
 // missing - fall back to selecting the text the old way.
 const r=document.createRange();r.selectNodeContents($("haYaml"));
 const sel=window.getSelection();sel.removeAllRanges();sel.addRange(r);
 try{document.execCommand("copy");msg(D[L].copied,"ok");}catch(e){}
}
async function saveWifi(){
 const r=await fetch("/api/wifi",{method:"POST",headers:{"Content-Type":"application/json"},
  body:JSON.stringify({ssid:$("ssid").value,pass:$("wpass").value})});
 msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");
}
async function geo(){
 const q=$("q").value.trim();if(!q)return;msg(D[L].searching);
 $("geoRow").classList.add("hide");$("geoSel").innerHTML="";
 try{const l=await(await fetch("/api/geocode?q="+encodeURIComponent(q))).json();
 if(!l.length){$("geoRow").classList.add("hide");$("geoSel").innerHTML="";msg(D[L].nothing,"warn");return;}
 $("geoRow").classList.remove("hide");
 $("geoSel").innerHTML=l.map(c=>"<option value='"+c.lat+","+c.lon+"'>"+c.name+(c.country?", "+c.country:"")+"</option>").join("");
 pickCity();msg("");}catch(e){$("geoRow").classList.add("hide");msg(D[L].failed,"err")}
}
function pickCity(){const v=$("geoSel").value.split(",");$("lat").value=(+v[0]).toFixed(4);$("lon").value=(+v[1]).toFixed(4);}
function exportCfg(){location="/api/export";}
async function importCfg(el){
 const f=el.files[0];if(!f)return;const t=await f.text();
 const r=await fetch("/api/import",{method:"POST",headers:{"Content-Type":"application/json"},
  body:JSON.stringify({password:$("adminPass").value,config:JSON.parse(t)})});
 msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");if(r.ok)load();
}
// JSON cards. A title is whatever the card's author typed, so it is escaped;
// an id is restricted by the device to a-z, 0-9 and "-".
function escHtml(t){return String(t).replace(/[&<>"']/g,c=>"&#"+c.charCodeAt(0)+";");}
async function cardsLoad(){
 try{const c=await(await fetch("/api/cards")).json();
 $("cardsTab").innerHTML=c.cards.length?c.cards.map(k=>{
  let h="";try{h=new URL(k.url).host}catch(e){}
  return "<tr><td><b>"+escHtml(k.title)+"</b><br><small>"+k.id+" · "+escHtml(h)+"</small></td><td>"+
   (k.ok?"OK":(k.tried?D[L].cardErr:D[L].cardWait))+"</td><td><button class='danger' onclick='cardDel(\""+k.id+
   "\")'>"+D[L].cardDel+"</button></td></tr>";}).join(""):"<tr><td>"+D[L].cardsNone+"</td></tr>";}catch(e){}
}
async function cardPost(u,b){
 b.password=$("adminPass").value;
 const r=await fetch(u,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(b)});
 if(r.ok)msg(D[L].saved,"ok");
 else{let e="";try{e=(await r.json()).error}catch(x){}msg(D[L].failed+(e?": "+e:" ("+r.status+")"),"err");}
 cardsLoad();
}
async function cardInstall(el){
 const f=el.files[0];if(!f)return;let j=null;
 try{j=JSON.parse(await f.text())}catch(e){}
 el.value="";
 if(!j){msg(D[L].cardBad,"err");return;}
 await cardPost("/api/cards",{card:j});
}
function cardDel(id){cardPost("/api/cards/delete",{id:id});}
async function doReboot(){await fetch("/api/reboot",{method:"POST"});msg("…");}
async function doReset(){
 if(!confirm(D[L].confirmReset))return;
 const r=await fetch("/api/reset",{method:"POST",headers:{"Content-Type":"application/json"},
  body:JSON.stringify({password:$("adminPass").value})});
 msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");
}
load();
</script></body></html>)rawliteral";
