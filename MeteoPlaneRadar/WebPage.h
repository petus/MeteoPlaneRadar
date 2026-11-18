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
input[type=time]{background:#0e1218;color:var(--fg);border:1px solid var(--line);border-radius:6px;
 padding:7px 8px;font-size:15px;color-scheme:dark}
.tpair{display:flex;align-items:center;gap:8px}
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
/* 0.8.6: a hint is one sentence; the rest folds away under "More". */
.hint details{display:inline}
.hint details[open]{display:block;margin-top:3px}
.hint summary{display:inline;cursor:pointer;color:var(--acc);list-style:none;margin-left:4px}
.hint summary::-webkit-details-marker{display:none}
.card h3{font-size:14px;margin:16px 0 4px;color:var(--fg)}
/* Three altitude boundaries side by side. The general min-width on number
   inputs would push this grid past the edge of a phone, hence the override. */
.abgrid{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:8px;margin:8px 0}
.abgrid label{display:flex;flex-direction:column;gap:4px;font-size:13px;color:var(--mut);min-width:0}
.abgrid input[type=number]{width:100%;min-width:0;flex:none}
input[type=file]{max-width:100%}
#iconLegend .chk{flex-wrap:nowrap}
.row.crow{flex-wrap:nowrap}
.row.crow label{flex:1 1 auto;min-width:0}
#iconLegend .chk span{flex:1 1 0;min-width:0}
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--bg);border-top:1px solid var(--line);
 padding:12px 16px calc(12px + env(safe-area-inset-bottom));display:flex;gap:12px;align-items:center;z-index:6}
/* Messages float above everything, the restart bar included. */
#msg{position:fixed;left:50%;transform:translateX(-50%);bottom:calc(16px + env(safe-area-inset-bottom));
 z-index:7;font-size:14px;background:#0e1218;border:1px solid var(--line);border-radius:999px;padding:7px 14px;
 max-width:calc(100% - 32px);text-align:center}
#msg:empty{display:none}
body.hasbar #msg{bottom:calc(76px + env(safe-area-inset-bottom))}
body.hasbar .wrap{padding-bottom:80px}
#rsBar{border-top:2px solid var(--warn)}
#rsBar span{flex:1 1 auto;font-size:14px;color:var(--warn)}
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
  <select id="uiLang" onchange="setLang(this.value)"><option value="0">Čeština</option><option value="2">Slovenčina</option><option value="1">English</option></select>
</header>

<nav class="tabs" id="tabs">
  <button data-tab="tCtl"    class="on" data-i18n="tabCtl">Ovládání</button>
  <button data-tab="tLoc"    data-i18n="tabLoc">Poloha</button>
  <button data-tab="tScr"    data-i18n="tabScr">Obrazovky</button>
  <button data-tab="tLook"   data-i18n="tabLook">Displej</button>
  <button data-tab="tClock"  data-i18n="tabClock">Hodiny</button>
  <button data-tab="tPlanes" data-i18n="tabPlanes">Letadla</button>
  <button data-tab="tMeteo"  data-i18n="tabMeteo">Meteoradar</button>
  <button data-tab="tEnergy" data-i18n="tabEnergy">Energie</button>
  <button data-tab="tPrint"  data-i18n="tabPrint">Tiskárna</button>
  <button data-tab="tFx"     data-i18n="tabFx">Kurzy &amp; Krypto</button>
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
    <p class="hint" data-i18n="restartHint"></p>
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
      <label class="chk"><input type="checkbox" id="sPrint"><span data-i18n="scrPrint">3D tisk</span></label>
      <label class="chk"><input type="checkbox" id="sFx"><span data-i18n="scrFx">Kurzy měn</span></label>
      <label class="chk"><input type="checkbox" id="sCrypto"><span data-i18n="scrCrypto">Kryptoměny</span></label>
    </div>
    <p class="hint" data-i18n="scrHint"></p>
    <p class="hint" data-i18n="restartHint"></p>
    <div class="row"><label data-i18n="autoRotate">Automatické střídání (sekundy, 0 = vypnuto)</label>
      <input type="number" id="autoRotate" min="0" max="3600" step="5"></div>
    <p class="hint" data-i18n="rotHint">Střídání pozastaví přejetí prstem, dlouhý stisk nebo přepnutí z prohlížeče — na trojnásobek intervalu, pak pokračuje samo. Obyčejné klepnutí ho nezastaví, otevřený detail letadla ho drží. Na obrazovce Nastavení se nestřídá.</p>
    <div class="row"><label data-i18n="dotsHide">Tečky obrazovek nahoře</label>
      <select id="dotsHide">
        <option value="0" data-i18n="dotsAlways">vždy zobrazené</option>
        <option value="3" data-i18n="dots3">skrýt po 3 s</option>
        <option value="5" data-i18n="dots5">skrýt po 5 s</option>
        <option value="10" data-i18n="dots10">skrýt po 10 s</option>
      </select></div>
    <p class="hint" data-i18n="dotsHint"></p>
  </div>
</section>

<!-- ================= DISPLEJ (Vzhled before 0.9.8) ================= -->
<section id="tLook" class="tab hide">
  <div class="card">
    <h2 data-i18n="brightness">Jas</h2>
    <div class="row"><label data-i18n="briDay">Denní jas</label><input type="range" id="briDay" min="10" max="100"><span id="briDayV"></span></div>
    <div class="row"><label data-i18n="briNight">Noční jas</label><input type="range" id="briNight" min="5" max="100"><span id="briNightV"></span></div>
    <div class="row"><label data-i18n="nightMode">Noční jas zapínat</label>
      <select id="nightMode">
        <option value="0" data-i18n="nmManual">ručně (klepnutím na hodiny)</option>
        <option value="1" data-i18n="nmSun">podle slunce</option>
        <option value="2" data-i18n="nmClock">podle času</option>
      </select></div>
    <div id="nmSunRow" class="row"><label data-i18n="nightOffset">Posun proti východu/západu (min)</label><input type="number" id="nightOffset" min="-120" max="120"></div>
    <div id="nmClockRow" class="row"><label data-i18n="nightWindow">Noc od – do</label>
      <span class="tpair"><input type="time" id="nightFrom"> – <input type="time" id="nightTo"></span></div>
    <p class="hint" data-i18n="nightHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="darkHdr">Vypnutí displeje v noci</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="darkOn"><span data-i18n="darkOn">Vypínat displej v noci</span></label></div>
    <div class="dep" data-dep="darkOn">
      <div class="row"><label data-i18n="darkWindow">Vypnuto od – do</label>
        <span class="tpair"><input type="time" id="darkFrom"> – <input type="time" id="darkTo"></span></div>
      <div class="row"><label class="chk"><input type="checkbox" id="darkAlerts"><span data-i18n="darkAlerts">Upozornění (letadlo, tisk) displej rozsvítí</span></label></div>
    </div>
    <p class="hint" data-i18n="darkHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="fontHdr">Písmo</h2>
    <div class="row"><label data-i18n="clockFont">Písmo (všechny obrazovky)</label>
      <select id="clockFont">
        <option value="1">Inter</option>
        <option value="2">Poppins</option>
        <option value="3" data-i18n="fontThin">Inter tenké</option>
        <option value="0" data-i18n="fontPixel">pixelové (původní)</option>
      </select></div>
  </div>
</section>

<!-- ================= HODINY (0.9.8: from Appearance) ================= -->
<section id="tClock" class="tab hide">
  <div class="card">
    <h2 data-i18n="clockHdr">Hodiny</h2>
    <div class="row"><label data-i18n="secStyle">Vteřinový prstenec</label>
      <select id="secStyle">
        <option value="0" data-i18n="secOff">vypnuto</option>
        <option value="1" data-i18n="secDots">tečky</option>
        <option value="2" data-i18n="secLine">čára</option>
        <option value="3" data-i18n="secComet">kometa</option>
      </select></div>
    <div class="row"><label data-i18n="clockColor">Barva hodin</label><input type="color" id="clockColor"></div>
    <div class="row"><label data-i18n="secColor">Barva vteřin</label><input type="color" id="secColor"></div>
    <div class="row"><label data-i18n="clockLayout">Rozložení hodin</label>
      <select id="clockLayout">
        <option value="0" data-i18n="clA">základní</option>
        <option value="1" data-i18n="clC">se Sluncem a Měsícem</option>
        <option value="2" data-i18n="clD">s předpovědí na příští hodiny</option>
      </select></div>
    <p class="hint" data-i18n="clockLayoutHint"></p>
    <div class="row"><label class="chk"><input type="checkbox" id="nameDay"><span data-i18n="nameDay">Zobrazit svátek nad datem</span></label></div>
    <p class="hint" data-i18n="nameDayHint"></p>
  </div>
</section>

<!-- ================= LETADLA ================= -->
<section id="tPlanes" class="tab hide">
  <div class="card">
    <h2 data-i18n="planes">Letadla</h2>
    <div class="row"><label data-i18n="altMin">Nejnižší výška</label><input type="number" id="altMin" min="0" max="60000"></div>
    <div class="row"><label data-i18n="altMax">Nejvyšší výška</label><input type="number" id="altMax" min="0" max="60000"></div>
    <div class="row"><label class="chk"><input type="checkbox" id="onlyCallsign"><span data-i18n="onlyCs">Jen letadla s callsignem</span></label></div>
    <div class="row"><label class="chk"><input type="checkbox" id="squawkAlert"><span data-i18n="sqAlert">Upozornit na nouzové squawky (7500/7600/7700)</span></label></div>
    <div class="row"><label data-i18n="watch">Sledovaný callsign nebo ICAO</label><input type="text" id="watch" maxlength="9" placeholder="CSA1234"></div>
    <p class="hint" data-i18n="planesHint">Filtry se týkají jen kreslení. Nouzový squawk ani sledované letadlo neschovají.</p>
  </div>

  <div class="card">
    <h2 data-i18n="planesView">Zobrazení radaru</h2>
    <div class="row"><label data-i18n="topBearing">Okno míří na (nahoře na radaru)</label>
      <select id="topBearing">
        <option value="0" data-i18n="dir0">sever (S)</option>
        <option value="45" data-i18n="dir45">severovýchod (SV)</option>
        <option value="90" data-i18n="dir90">východ (V)</option>
        <option value="135" data-i18n="dir135">jihovýchod (JV)</option>
        <option value="180" data-i18n="dir180">jih (J)</option>
        <option value="225" data-i18n="dir225">jihozápad (JZ)</option>
        <option value="270" data-i18n="dir270">západ (Z)</option>
        <option value="315" data-i18n="dir315">severozápad (SZ)</option>
      </select></div>
    <div class="row"><label class="chk"><input type="checkbox" id="metric"><span data-i18n="metric">Metrické jednotky (m, km/h)</span></label></div>
    <p class="hint" data-i18n="planesViewHint"></p>
    <div class="row"><label class="chk"><input type="checkbox" id="showRoute"><span data-i18n="showRoute">Zobrazit trasu (odkud – kam) v detailu letadla</span></label></div>
    <p class="hint" data-i18n="showRouteHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="altHdr">Barvy podle výšky</h2>
    <div class="abgrid">
      <label><span data-i18n="altB1">1. hranice</span><input type="number" id="ab0" min="100" max="20000" step="100"></label>
      <label><span data-i18n="altB2">2. hranice</span><input type="number" id="ab1" min="100" max="20000" step="100"></label>
      <label><span data-i18n="altB3">3. hranice</span><input type="number" id="ab2" min="100" max="20000" step="100"></label>
    </div>
    <div class="row crow"><label id="al0"></label><input type="color" id="ac0"></div>
    <div class="row crow"><label id="al1"></label><input type="color" id="ac1"></div>
    <div class="row crow"><label id="al2"></label><input type="color" id="ac2"></div>
    <div class="row crow"><label id="al3"></label><input type="color" id="ac3"></div>
    <div class="row"><button class="sec" onclick="altDefaults()" data-i18n="altDef">Výchozí barvy a hranice</button></div>
    <p class="hint" data-i18n="altHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="winHdr">Výhled z okna</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="windowOn"><span data-i18n="winOn">Používat výhled z okna</span></label></div>
    <div class="dep" data-dep="windowOn">
      <div class="row"><label data-i18n="winW">Šířka výhledu (°)</label><input type="number" id="windowWidth" min="10" max="300" step="5"></div>
      <div class="row"><label data-i18n="winKm">Dohled (km)</label><input type="number" id="windowKm" min="1" max="100" step="1"></div>
    </div>
    <p class="hint" data-i18n="winHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="paHdr">Letadlo poblíž</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="planeAlert"><span data-i18n="planeAlert">Ukázat letadla jen když je letadlo blízko</span></label></div>
    <div class="dep" data-dep="planeAlert">
      <div class="row"><label data-i18n="paKm">Do vzdálenosti (km)</label><input type="number" id="planeAlertKm" min="1" max="50" step="1"></div>
      <div class="row"><label data-i18n="paSec">Ukázat na (sekundy)</label><input type="number" id="planeAlertSec" min="5" max="300" step="5"></div>
      <div class="row"><label data-i18n="paMode">Co ukázat</label>
        <select id="planeAlertMode">
          <option value="0" data-i18n="paMode0">Hned detail letadla</option>
          <option value="1" data-i18n="paMode1">Nejdřív radar, pak detail</option>
          <option value="2" data-i18n="paMode2">Jen radar s označeným letadlem</option>
        </select></div>
      <p class="hint" data-i18n="paModeHint"></p>
    </div>
    <p class="hint" data-i18n="paHint"></p>
  </div>

  <div class="card">
    <h2 data-i18n="iconHdr">Ikony letadel</h2>
    <p class="hint" data-i18n="iconIntro"></p>
    <div id="iconLegend" style="display:grid;grid-template-columns:repeat(auto-fill,minmax(230px,1fr));gap:6px 14px;margin-top:10px"></div>
    <p class="hint" data-i18n="iconCredit"></p>
  </div>
</section>

<!-- ================= METEORADAR (0.9.8: from Screens) ================= -->
<section id="tMeteo" class="tab hide">
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
    <p class="hint" data-i18n="restartHint"></p>
    <div class="row"><label class="chk"><input type="checkbox" id="meteoLegend"><span data-i18n="mtLegend">Zobrazit legendu (dBZ / mm/h)</span></label></div>
    <p class="hint" data-i18n="mtLegendHint">Legenda zabírá levý okraj mapy. Když stupnici znáte, dá se skrýt a je vidět víc území.</p>

    <h3 data-i18n="stormHdr">Meteoradar jen při bouřce</h3>
    <div class="row"><label class="chk"><input type="checkbox" id="stormOnly"><span data-i18n="stormOnly">Meteoradar ve střídání jen když se blíží srážky/bouřka</span></label></div>
    <div class="dep" data-dep="stormOnly">
      <div class="row"><label data-i18n="stormDbz">Odraz aspoň (dBZ)</label><input type="number" id="stormDbz" min="4" max="60" step="1"></div>
      <div class="row"><label data-i18n="stormKm">Do vzdálenosti (km)</label><input type="number" id="stormKm" min="5" max="250" step="5"></div>
    </div>
    <p class="hint" data-i18n="stormHint"></p>
  </div>
</section>

<!-- ================= ENERGIE ================= -->
<section id="tEnergy" class="tab hide">
  <div class="card" id="cardPrice">
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

  <div class="card" id="cardMix">
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

<!-- ================= TISKÁRNA (0.9.0) ================= -->
<section id="tPrint" class="tab hide">
  <div class="card">
    <h2 data-i18n="printHdr">3D tiskárna</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="printOn"><span data-i18n="printOn">Hlídat 3D tiskárnu</span></label></div>
    <div class="dep" data-dep="printOn">
      <div class="row"><label data-i18n="printType">Typ</label>
        <select id="printType"><option value="0">Prusa (PrusaLink)</option><option value="1">Klipper (Moonraker)</option></select></div>
      <div class="row"><label data-i18n="printHost">Adresa (IP)</label>
        <input type="text" id="printHost" placeholder="192.168.1.50" autocomplete="off" spellcheck="false"></div>
      <p class="hint" data-i18n="printHostHint"></p>
      <div class="prusaOnly">
        <div class="row"><label data-i18n="printUser">Uživatel</label>
          <input type="text" id="printUser" placeholder="maker" autocomplete="off" spellcheck="false"></div>
        <div class="row"><label data-i18n="printPass">Heslo</label>
          <input type="password" id="printPass" autocomplete="new-password"></div>
      </div>
      <div class="row"><label data-i18n="printKey">API klíč</label>
        <input type="password" id="printKey" autocomplete="off"></div>
      <p class="hint" id="printSecretState"></p>
      <p class="hint" data-i18n="printAuthHint"></p>
      <div class="row"><label class="chk"><input type="checkbox" id="printExport"><span data-i18n="printExport">Heslo a API klíč i do exportu a importu nastavení</span></label></div>
      <p class="hint warn" data-i18n="printExportHint"></p>
      <div class="row">
        <button onclick="savePrint()" data-i18n="printSave">Uložit tiskárnu</button>
        <button class="sec" onclick="forgetPrint()" data-i18n="printForget">Zapomenout heslo a klíč</button>
      </div>
      <p><b data-i18n="printNow">Stav:</b> <span id="printSt">–</span></p>
    </div>
    <p class="hint" data-i18n="printIntro"></p>
  </div>

  <div class="card">
    <h2 data-i18n="printAlertHdr">Upozornění</h2>
    <div class="row"><label class="chk"><input type="checkbox" id="printAlertFinish"><span data-i18n="printAlertFinish">Tisk je hotový</span></label></div>
    <div class="row"><label class="chk"><input type="checkbox" id="printAlertError"><span data-i18n="printAlertError">Chyba tisku</span></label></div>
    <div class="row"><label class="chk"><input type="checkbox" id="printAlertPause"><span data-i18n="printAlertPause">Tisk se pozastavil</span></label></div>
    <div class="row"><label data-i18n="printNight">V noci</label>
      <select id="printNight">
        <option value="0" data-i18n="printNight0">noční jas, blikat</option>
        <option value="1" data-i18n="printNight1">plný jas, blikat</option>
        <option value="2" data-i18n="printNight2">noční jas, neblikat</option>
      </select></div>
    <p class="hint" data-i18n="printAlertHint"></p>
  </div>
</section>

<!-- ================= KURZY (0.9.6) ================= -->
<section id="tFx" class="tab hide">
  <div class="card">
    <h2 data-i18n="fxHdr">Kurzy měn</h2>
    <p class="hint" data-i18n="fxIntro"></p>
    <div class="row"><label data-i18n="fxHome">Domácí měna</label>
      <select id="fxHome" style="flex:2 1 220px"></select></div>
    <p class="hint" data-i18n="fxAutoHint"></p>
    <p class="hint" id="fxSrc"></p>
    <h3 data-i18n="fxList">Zobrazované měny</h3>
    <div class="grid" id="fxGrid" style="margin-top:8px"></div>
    <p class="hint" id="fxCount"></p>
    <div class="row"><button class="sec" onclick="fxSave({fxList:''})" data-i18n="fxDefault">Výchozí výběr</button></div>
    <p class="hint" data-i18n="fxListHint"></p>
  </div>
  <div class="card" id="cardCrypto">
    <h2 data-i18n="crHdr">Kryptoměny</h2>
    <p class="hint" data-i18n="crIntro"></p>
    <h3 data-i18n="crList">Zobrazované mince</h3>
    <div class="grid" id="crGrid" style="margin-top:8px"></div>
    <p class="hint" id="crCount"></p>
    <div class="row"><button class="sec" onclick="fxSave({crList:''})" data-i18n="fxDefault">Výchozí výběr</button></div>
    <h3 data-i18n="crKey">API klíč CoinGecko (nepovinný)</h3>
    <div class="row"><input type="password" id="cgKey" autocomplete="off" spellcheck="false" style="flex:2 1 220px">
      <button class="sec" onclick="fxSave({cgKey:$('cgKey').value.trim()})" data-i18n="crKeySave">Uložit klíč</button>
      <button class="sec" id="cgKeyDel" onclick="fxSave({cgKey:''})" data-i18n="crKeyDel">Smazat</button></div>
    <p class="hint" id="cgKeySt"></p>
    <p class="hint" data-i18n="crKeyHint"></p>
    <p class="hint" data-i18n="crSrcHint"></p>
  </div>
</section>

<!-- ================= HOME ASSISTANT ================= -->
<section id="tHa" class="tab hide">
  <div class="card">
    <h2>Home Assistant</h2>
    <p class="hint warn" data-i18n="haUntested"></p>
    <div class="row"><label class="chk"><input type="checkbox" id="haEnabled"><span data-i18n="haOn">Zpřístupnit data pro Home Assistant</span></label></div>
    <p class="hint" data-i18n="haHow"></p>
    <div class="dep" data-dep="haEnabled">
      <p><b id="haUrl"></b> &nbsp;<a id="haLink" target="_blank" rel="noopener" data-i18n="haOpen">otevřít</a></p>
      <p class="hint" data-i18n="haYamlHint"></p>
      <pre id="haYaml" style="background:#0e1218;border:1px solid var(--line);border-radius:6px;padding:10px;overflow-x:auto;font-size:12px;white-space:pre"></pre>
      <button class="sec" onclick="copyHa()" data-i18n="haCopy">Zkopírovat</button>
    </div>
  </div>
</section>

<!-- ================= SYSTÉM ================= -->
<section id="tSys" class="tab hide">
  <div class="card">
    <h2 data-i18n="system">Systém</h2>
    <div class="row"><label data-i18n="adminPass">Heslo správce</label>
      <input type="password" id="adminPass" placeholder="•••" autocomplete="current-password"></div>
    <p class="hint" id="pwState"></p>
    <p class="hint" data-i18n="passHint"></p>
    <div class="row" style="margin-top:12px">
      <button class="sec" onclick="location='/update'" data-i18n="fwUpdate">Aktualizace firmwaru</button>
      <button class="sec" onclick="exportCfg()" data-i18n="export">Export nastavení</button>
      <button class="sec" onclick="document.getElementById('imp').click()" data-i18n="import">Import</button>
      <input type="file" id="imp" class="hide" accept="application/json" onchange="importCfg(this)">
      <button class="sec" onclick="doReboot()" data-i18n="reboot">Restart</button>
      <button class="danger" onclick="doReset()" data-i18n="factory">Tovární reset</button>
    </div>
  </div>

  <div class="card">
    <h2 data-i18n="pwChangeHdr">Změnit heslo správce</h2>
    <div class="row"><label data-i18n="pwCur">Současné heslo</label>
      <input type="password" id="pwCur" placeholder="•••" autocomplete="current-password"></div>
    <div class="row"><label data-i18n="newPass">Nové heslo</label>
      <input type="password" id="newPass" autocomplete="new-password"></div>
    <div class="row"><label data-i18n="newPass2">Nové heslo znovu</label>
      <input type="password" id="newPass2" autocomplete="new-password"></div>
    <div class="row"><button class="sec" onclick="changePw()" data-i18n="pwChange">Změnit heslo</button></div>
    <p class="hint" data-i18n="pwChangeHint"></p>
    <p class="hint" data-i18n="passNote"></p>
  </div>

  <div class="card">

    <!-- Data sources. This is not decoration: GeoNames, CHMI, RainViewer,
         Open-Meteo and Energy-Charts all require their source to be credited,
         and RainViewer asks for a link back. The 480x480 panel has no room for
         a legible credit line, so this panel is where the project discharges
         that obligation. Do not delete it in a fork - replace it. -->
    <h2 data-i18n="srcTitle">Zdroje dat</h2>
    <p class="hint" data-i18n="srcIntro">Data pocházejí z těchto služeb. Několik z nich vyžaduje uvedení zdroje, proto jsou uvedeny zde. Podrobnosti a podmínky jsou v souboru LICENSE.txt.</p>
    <p class="hint">
      <a href="https://adsb.fi" target="_blank" rel="noopener">adsb.fi</a> ·
      <a href="https://adsb.lol" target="_blank" rel="noopener">adsb.lol</a>, <a href="https://www.adsbdb.com" target="_blank" rel="noopener">adsbdb</a> ·
      <a href="https://opendata.chmi.cz" target="_blank" rel="noopener">ČHMÚ</a> ·
      <a href="https://www.rainviewer.com" target="_blank" rel="noopener">RainViewer</a> ·
      <a href="https://open-meteo.com" target="_blank" rel="noopener">Open-Meteo</a> ·
      <a href="https://spotovaelektrina.cz" target="_blank" rel="noopener">spotovaelektrina.cz</a> (OTE) ·
      <a href="https://energy-charts.info" target="_blank" rel="noopener">Energy-Charts.info</a> (Fraunhofer ISE, CC BY 4.0) ·
      <span data-i18n="srcFx">kurzy</span>: <a href="https://www.cnb.cz" target="_blank" rel="noopener">ČNB</a>,
      <a href="https://data.ecb.europa.eu" target="_blank" rel="noopener">ECB</a> ·
      <span data-i18n="srcCrypto">kryptoměny</span>: <a href="https://www.coingecko.com/en/api" target="_blank" rel="noopener">Data provided by CoinGecko</a> ·
      <a href="https://www.geonames.org" target="_blank" rel="noopener">GeoNames</a> (CC BY 4.0) ·
      Natural Earth ·
      <a href="http://ip-api.com" target="_blank" rel="noopener">ip-api.com</a> ·
      <span data-i18n="srcNameDays">svátky</span>: <a href="https://github.com/OzzyCzech/namedays-cs" target="_blank" rel="noopener">namedays-cs</a> (Roman Ožana, MIT),
      <a href="https://github.com/jozefgrencik/slovak-name-days" target="_blank" rel="noopener">slovak-name-days</a> (Jozef Grenčík, MIT) ·
      <span data-i18n="srcIcons">ikony letadel podle</span> <a href="https://adsb-radar.com" target="_blank" rel="noopener">ADS-B Radar</a>
    </p>
  </div>
</section>

</div>

<div class="bar hide" id="rsBar"><span id="rsTxt"></span><button onclick="restartNow()" data-i18n="rsNow">Restartovat teď</button></div>
<div id="msg"></div>

<script>
const D={
 cs:{tabCtl:"Ovládání",tabLoc:"Poloha",tabScr:"Obrazovky",tabLook:"Displej",tabClock:"Hodiny",tabMeteo:"Meteoradar",fontHdr:"Písmo",tabPlanes:"Letadla",tabEnergy:"Energie",
  tabSys:"Systém",scrPrice:"Cena elektřiny",scrMix:"Výroba ČR",energyPrice:"Cena elektřiny",
  energyMix:"Výroba ČR",mixCountry:"Země",
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
  wifiNow:"Připojeno k síti",autoSaved:"Uloženo",
  restartHint:"Změna se uloží hned, projeví se ale až po restartu zařízení. Dole se objeví pruh s tlačítkem Restartovat teď; když ho nepoužijete, zařízení se restartuje samo 2 minuty po poslední takové změně, takže stihnete změnit víc věcí najednou.",
  rsTxt:"Změny se projeví po restartu. Zařízení se restartuje samo za {t}.",rsNow:"Restartovat teď",rsGoing:"Restartuji, stránka se za chvíli načte znovu…",
  remote:"Dálkové ovládání",rangeLbl:"Rozsah",statusHdr:"Stav zařízení",
  remoteHint:"Rozsah se mění jen na obrazovkách Letadla a Meteoradar. Zásah pozastaví automatické střídání.",
  location:"Poloha",findCity:"Najít město",search:"Hledat",found:"Nalezeno",lat:"Zeměpisná šířka",
  lon:"Zeměpisná délka",
  screens:"Obrazovky",scrClock:"Hodiny",scrPlanes:"Letadla",scrMeteo:"Meteoradar",scrForecast:"Předpověď",
  board:"Deska",scrHint:"Vypnuté obrazovky se přeskakují a jejich záložka tady na webu se skryje. Letadla, Meteoradar a Tiskárna zůstanou vidět i s vypnutou obrazovkou, když je zapnuté to, co funguje i bez ní: letadlo poblíž, meteoradar při bouřce nebo hlídání tiskárny. Obrazovka Nastavení na displeji je dostupná vždy.",
  autoRotate:"Automatické střídání (sekundy, 0 = vypnuto)",
  dotsHide:"Tečky obrazovek nahoře",dotsAlways:"vždy zobrazené",dots3:"skrýt po 3 s",dots5:"skrýt po 5 s",dots10:"skrýt po 10 s",
  dotsHint:"Tečky ukazují, na které obrazovce jste. Po přepnutí nebo dotyku se ukážou a po zvolené době zmizí. Na obrazovce Nastavení zůstávají vždy, při upozornění na letadlo nebo tisk se neukazují a v noci je nepřinese automatické střídání, jen dotyk, dlouhý stisk nebo přepnutí z prohlížeče.",
  rotHint:"Střídání pozastaví přejetí prstem, dlouhý stisk nebo přepnutí z prohlížeče — na trojnásobek intervalu, pak pokračuje samo. Obyčejné klepnutí ho nezastaví, otevřený detail letadla ho drží. Na obrazovce Nastavení se nestřídá.",
  radar:"Meteoradar",radarSrc:"Zdroj dat",srcChmu:"ČHMÚ (ostřejší, jen ČR)",srcRv:"RainViewer (Evropa i svět)",
  mtLegend:"Zobrazit legendu (dBZ / mm/h)",
  mtLegendHint:"Legenda zabírá levý okraj mapy. Když stupnici znáte, dá se skrýt a je vidět víc území.",
  radarHint:"Mimo ČR nemá ČHMÚ data a obrazovka zůstane prázdná — použijte RainViewer.",brightness:"Jas",
  clockHdr:"Hodiny",briDay:"Denní jas",briNight:"Noční jas",nightAuto:"Přepínat automaticky podle slunce",
  nightMode:"Noční jas zapínat",nmManual:"ručně (klepnutím na hodiny)",nmSun:"podle slunce",nmClock:"podle času",
  nightWindow:"Noc od – do",
  nightHint:"Podle slunce se noc řídí západem a východem slunce v místě zařízení, posun ji prodlouží nebo zkrátí. Podle času platí noc každý den ve zvoleném rozmezí, může přes půlnoc (22:00 – 6:00).",
  darkHdr:"Vypnutí displeje v noci",darkOn:"Vypínat displej v noci",darkWindow:"Vypnuto od – do",
  darkAlerts:"Upozornění (letadlo, tisk) displej rozsvítí",
  darkHint:"Ve zvoleném čase displej zhasne úplně, zařízení ale běží dál a data stahuje. Dotyk ho rozsvítí na 30 sekund, první dotyk jen rozsvěcí a nic nepřepne. Se zaškrtnutými upozorněními displej rozsvítí i blízké letadlo nebo tiskárna; bez nich zůstane tma, upozornění na letadlo se vynechá a nevyřízené upozornění od tiskárny ráno ukáže barevná tečka na hodinách.",
  nameDay:"Zobrazit svátek nad datem",
  clockLayout:"Rozložení hodin",clA:"základní",clC:"se Sluncem a Měsícem",clD:"s předpovědí na příští hodiny",
  clockLayoutHint:"Základní: datum, čas, počasí a vítr rozložené po celém displeji. Se Sluncem a Měsícem: v řádku počasí dnešní max/min, dole východ a západ Slunce a Měsíce; ikona Měsíce ukazuje jeho fázi. S předpovědí: dole počasí za 2, 4 a 6 hodin. U dvou posledních je vítr v řádku s počasím, a když se tam nevejde, vynechá se – srážky mají přednost. Slunce i Měsíc platí pro polohu zařízení z nastavení; Měsíc se počítá přímo v zařízení.",
  nameDayHint:"Česky český kalendář, slovensky slovenský. V angličtině se svátek nezobrazuje. Ve státní svátek bez jména se ukáže název svátku.",
  nightOffset:"Posun proti východu/západu (min)",secStyle:"Vteřinový prstenec",secOff:"vypnuto",secDots:"tečky",
  secLine:"čára",secComet:"kometa",clockColor:"Barva hodin",secColor:"Barva vteřin",
  metric:"Metrické jednotky (m, km/h)",topBearing:"Okno míří na (nahoře na radaru)",planes:"Letadla",
  altMin:"Nejnižší výška ({u})",altMax:"Nejvyšší výška ({u})",onlyCs:"Jen letadla s callsignem",
  sqAlert:"Upozornit na nouzové squawky (7500/7600/7700)",watch:"Sledovaný callsign nebo ICAO",
  planesHint:"Filtry se týkají jen kreslení. Nouzový squawk ani sledované letadlo neschovají.",
  planesView:"Zobrazení radaru",
  showRoute:"Zobrazit trasu (odkud – kam) v detailu letadla",
  showRouteHint:"Trasa se dohledává podle callsignu ve veřejných databázích (adsb.lol a adsbdb; když jedna nefunguje, zařízení přejde na druhou) a občas nesedí. Letecké společnosti používají stejný callsign na různých linkách a databáze to nemusí stihnout. Vypnuté se trasa nezobrazí ani nedohledává.",
  planesViewHint:"Radar se natočí tak, aby směr, kterým se díváte z okna, byl nahoře. Stejný směr používá i výhled z okna níže. Mění se i tlačítky − a + na obrazovce Nastavení. Meteoradar se záměrně neotáčí, srážková mapa se čte severem nahoru. Jednotky platí pro detail letadla.",
  pwNone:"Zatím není nastavené žádné heslo — aktualizace, import a reset jsou otevřené.",
  pwSet:"Heslo je nastavené.",
  passNote:"Poznámka: výchozí stav je bez hesla — aktualizace, import i reset jsou pak přístupné komukoli v síti. Heslo se ukládá v čitelné podobě, protože aktualizační stránka používá HTTP Basic, kde otisk použít nelze; chrání tedy před domácností, ne před útočníkem. Zapomenuté heslo jde zrušit jedině podržením tlačítka BOOT při startu (~3 s), což smaže i WiFi a všechna ostatní nastavení.",
  srcTitle:"Zdroje dat",
  srcIntro:"Data pocházejí z těchto služeb. Několik z nich vyžaduje uvedení zdroje, proto jsou uvedeny zde. Podrobnosti a podmínky jsou v souboru LICENSE.txt.",
  system:"Systém",adminPass:"Heslo správce",newPass:"Nové heslo",
  passHint:"Vyplňte, jen když je heslo nastavené. Potřebuje ho aktualizace firmwaru, import nastavení a tovární reset; samotné ukládání nastavení heslo nevyžaduje a nikdy ho nemění.",
  fwUpdate:"Aktualizace firmwaru",export:"Export nastavení",import:"Import",reboot:"Restart",
  factory:"Tovární reset",settings:"Nastavení",saved:"Uloženo",failed:"Nepovedlo se",
  searching:"Hledám…",nothing:"Nic nenalezeno",disabled:"Obrazovka je vypnutá",
  confirmReset:"Opravdu smazat všechna nastavení včetně WiFi?",tabHa:"Home Assistant",wifiSaved:"Uložené sítě",
  wifiSavedHint:"Zařízení si pamatuje až tři sítě. Lze je zapsat ručně i když zrovna nejsou v dosahu — třeba síť v kanceláři ještě doma. Po výpadku spojení zkusí zařízení po minutě ostatní uložené sítě, nejsilnější jako první. Prázdné pole hesla nechá uložené heslo beze změny. Uložení sítí zařízení neodpojí.",
  wifiSaveList:"Uložit sítě",passStored:"heslo uloženo",passNone:"heslo",ssidPh:"název sítě",
  stormHdr:"Meteoradar jen při bouřce",stormOnly:"Meteoradar ve střídání jen když se blíží srážky/bouřka",
  stormDbz:"Odraz aspoň (dBZ)",stormKm:"Do vzdálenosti (km)",
  stormHint:"Meteoradar se do střídání přidá jen tehdy, když se blíží srážky. Každých 5 minut se zkontroluje nejnovější snímek radaru ČHMÚ; když je v zadané vzdálenosti odraz aspoň zadané síly, meteoradar se přidá do střídání a po dvou klidných snímcích zase vypadne. Funguje i když je obrazovka Meteoradar výše vypnutá. Orientačně: 20 dBZ slabý déšť, 40 dBZ silný déšť, nad 50 dBZ bouřka s možným krupobitím. Data jsou jen z ČHMÚ, takže v ČR a kousek za hranicemi. Potřebuje zapnuté automatické střídání.",
  paHdr:"Letadlo poblíž",planeAlert:"Ukázat letadla jen když je letadlo blízko (nebo ve výhledu z okna)",
  paKm:"Do vzdálenosti (km)",paSec:"Ukázat na (sekundy)",
  paMode:"Co ukázat",paMode0:"Hned detail letadla",paMode1:"Nejdřív radar, pak detail",paMode2:"Jen radar s označeným letadlem",paModeHint:"Na radaru má letadlo tyrkysový kroužek a pod hodinami je vidět, jak je daleko. Při volbě „nejdřív radar“ se radar ukáže zhruba na čtvrtinu nastavené doby (nejméně 5 s, nejvýš 15 s) a zbytek patří detailu.",
  paHint:"Když se letadlo přiblíží, zařízení na něj na chvíli přepne. Obrazovka Letadla se pak sama neobjevuje ve střídání: jakmile se letadlo přiblíží na zadanou vzdálenost, zařízení na ni přepne, ukáže to letadlo (radar, detail nebo obojí po sobě) a po zadané době se vrátí tam, kde bylo. Každé letadlo se ukáže jen jednou (za 15 minut). Funguje i bez automatického střídání. Výhled z okna (výše) má přednost — letadlo, které z okna vidíte, se ukáže, i když je dál než jiné za domem. Dotyk během ukázky ji ukončí a zůstane se na letadlech.",
  srcChmuM:"ČHMÚ masked (jen srážky dopadající na zem)",
  srcDiff:"ČHMÚ ukazuje všechno, co radary vidí, plnou barvou — včetně srážek, které se vypaří dřív, než dopadnou, a kovadlin vzdálených bouřek. ČHMÚ masked je tentýž snímek, ale plnou barvou jen tam, kde podle měření ve výšce 2 km srážky opravdu padají na zem; zbytek je bledší. Pro otázku „bude u nás pršet?“ je masked přesnější, pro sledování bouřek je lepší plný.",
  clockFont:"Písmo (všechny obrazovky)",fontThin:"Inter tenké",fontPixel:"pixelové (původní)",
  altHdr:"Barvy podle výšky",altBelow:"pod {a}",altB1:"1. hranice ({u})",altB2:"2. hranice ({u})",
  altB3:"3. hranice ({u})",altDef:"Výchozí barvy a hranice",
  altHint:"Tři hranice rozdělují výšky do čtyř pásem a každé pásmo má svou barvu. Výchozí je červená pod 2 km, oranžová 2–6 km, žlutá 6–10 km a modrá nad 10 km. Hranice musí jít vzestupně. Hranice i legenda na displeji jsou v metrech, nebo ve stopách podle volby Metrické jednotky výše.",
  altBad:"Hranice musí jít vzestupně",winHdr:"Výhled z okna",winOn:"Používat výhled z okna",
  dir0:"sever (S)",dir45:"severovýchod (SV)",dir90:"východ (V)",dir135:"jihovýchod (JV)",dir180:"jih (J)",
  dir225:"jihozápad (JZ)",dir270:"západ (Z)",dir315:"severozápad (SZ)",winW:"Šířka výhledu (°)",
  winKm:"Dohled (km)",
  winHint:"Výseč oblohy, kterou z okna vidíte. Kam okno míří, se nastavuje výše v Zobrazení radaru; tady jen jak široký úhel oblohy z okna vidíte (výseč je rozložená rovnoměrně kolem směru okna — 90° u okna na SV pokryje sever až východ) a jak daleko letadlo ještě rozeznáte. Na radaru letadel se zakreslí tenkými čarami. Letadlo ve výhledu má při upozornění přednost před bližším letadlem, které z okna vidět není.",
  haUntested:"Tato funkce zatím nebyla vyzkoušena se skutečným Home Assistantem. Formát odpovídá dokumentaci integrace RESTful, ale pokud něco nesedí, dejte vědět.",
  haOn:"Zpřístupnit data pro Home Assistant",
  haHow:"Home Assistant si data sám stahuje (zhruba jednou za minutu) z adresy níže. Nic se nikam neposílá a nepotřebujete MQTT. Data jsou jen ke čtení: teplota, vítr, srážky, bouřka v okolí, nejbližší letadlo, cena elektřiny. Dokud je to zapnuté, zařízení hlídá i bouřku (jeden snímek ČHMÚ za 5 minut) a letadla poblíž (malý dotaz každých 30 s).",
  haOpen:"otevřít",
  haYamlHint:"Do souboru configuration.yaml v Home Assistantu vložte tohle a Home Assistant restartujte:",
  haCopy:"Zkopírovat",copied:"Zkopírováno",stormSt:"Bouřka",planeWatchSt:"Letadla poblíž",
  srcNameDays:"svátky",srcIcons:"ikony letadel podle",iconHdr:"Ikony letadel",
  iconIntro:"Tvar ikony na radaru podle typu letadla, velikost odpovídá hmotnosti. Typ se pozná z kódu typu (A20N, B738, EC35…), u neznámého z kategorie, kterou vysílá transpondér. Barva ikony udává výšku (viz Barvy podle výšky výše).",
  iconCredit:"Rozdělení typů letadel do ikon vychází ze schématu ADS-B Radar (adsb-radar.com/help/icons.html). Tvary kreslí firmware sám, soubory ikon se nepřebírají.",
  icNarrow:"úzkotrupé — A320, B737, B757",icWide:"širokotrupé dvoumotorové — A330, A350, B767, B777, B787",
  icQuad:"čtyřmotorové — A340, A380, B747",icRegional:"regionální — CRJ, Embraer E-Jet, Fokker 100",
  icBizjet:"business jet — Citation, Learjet, Falcon, Gulfstream",
  icTwinprop:"turbovrtulové — ATR, Dash 8, King Air, L 410",
  icLight:"malá letadla — Cessna, Piper, Zlin, ultralighty",
  icMiltrans:"vojenská transportní — C-130, C-160, A400M",
  icFreighter:"těžká nákladní — An-124, C-17, C-5, Il-76",icFighter:"stíhačky a cvičné proudové",
  icGlider:"kluzáky a motorové kluzáky",icTilt:"konvertoplány — V-22, AW609",icHeli:"vrtulníky",
  icBalloon:"balony a vzducholodě",icDrone:"drony",icGeneric:"nezjištěný typ",icNoTrack:"neznámý směr letu",
  more:"Více",less:"Méně",pwChangeHdr:"Změnit heslo správce",pwCur:"Současné heslo",newPass2:"Nové heslo znovu",
  pwChange:"Změnit heslo",
  pwChangeHint:"Současné heslo vyplňte, jen když už nějaké nastavené je. Prázdné nové heslo ochranu vypne.",
  pwMismatch:"Nová hesla se neshodují",
  pwOffConfirm:"Prázdné heslo vypne ochranu aktualizace, importu a resetu. Pokračovat?",
  pwWrong:"Špatné heslo správce",pwChanged:"Heslo změněno",pwRemoved:"Ochrana heslem je vypnutá",
  inclVat:" včetně DPH",altAbove:"nad {a}",
  scrCrypto:"Kryptoměny",crHdr:"Kryptoměny",crList:"Zobrazované mince",crCount:"Vybráno {n} z nejvýše 6.",crKey:"API klíč CoinGecko (nepovinný)",crKeySave:"Uložit klíč",crKeyDel:"Smazat",crKeyOn:"Klíč je uložený (z bezpečnosti se nezobrazuje).",crKeyOff:"Bez klíče – stačí to.",srcCrypto:"kryptoměny",crSt:"Kryptoměny",
  crIntro:"Obrazovka Kryptoměny ukazuje cenu vybraných mincí ve stejné domácí měně jako kurzy (nahoře na této záložce) a šipkou změnu za 24 hodin. Ve výchozím stavu je vypnutá, zapnete ji na záložce Obrazovky.",
  crKeyHint:"Bez klíče CoinGecko povoluje zhruba 5 dotazů za minutu na IP adresu, zařízení se ptá jednou za 10 minut. Klíč (bezplatný Demo) pomůže, jen když je za stejnou IP víc zařízení. Klíč se do exportu nastavení neukládá.",
  crSrcHint:"Zdroj: CoinGecko, průměr z burz – orientační cena, ne cena konkrétního nákupu. Když CoinGecko neodpovídá, zkouší to zařízení znovu za 2, 4, 8, 16 a pak každých 30 minut; čas nahoře zoranžoví, když data nejsou čerstvá, a po 6 hodinách zešednou i ceny.",
  tabFx:"Kurzy & Krypto",scrFx:"Kurzy měn",fxHdr:"Kurzy měn",fxHome:"Domácí měna",fxAuto:"Automaticky",fxList:"Zobrazované měny",fxDefault:"Výchozí výběr",fxCount:"Vybráno {n} z nejvýše 6.",srcFx:"kurzy",fxSt:"Kurzy měn",
  fxIntro:"Obrazovka Kurzy měn ukazuje, kolik stojí vybrané měny v domácí měně, a šipkou změnu proti předchozímu kurzu. Ve výchozím stavu je vypnutá, zapnete ji na záložce Obrazovky.",
  fxAutoHint:"Automaticky znamená: čeština – koruna, slovenština – euro, angličtina – měna země vybrané u výroby elektřiny (jinak euro).",
  fxSrcCnb:"Zdroj: Česká národní banka, kurzy devizového trhu. Vyhlašují se v pracovní dny kolem 14:30, o víkendu a ve svátek platí poslední kurz.",
  fxSrcEcb:"Zdroj: Evropská centrální banka, referenční kurzy. Vyhlašují se v pracovní dny kolem 16:00; kurz mezi dvěma měnami jinými než euro se přepočítá přes euro.",
  fxListHint:"Nejvýše šest měn, domácí měna vybrat nejde. Měny s malou hodnotou jednotky se ukazují za 100 nebo 1000 kusů (třeba 100 HUF), stejně jako na kurzovním lístku. Domácí měna i výběr jsou součástí exportu a importu nastavení.",
  routeSt:"Trasa letu",tabPrint:"Tiskárna",scrPrint:"3D tisk",printHdr:"3D tiskárna",printOn:"Hlídat 3D tiskárnu",printType:"Typ",printHost:"Adresa (IP)",printHostHint:"Zadejte IP adresu tiskárny, název s .local funguje taky, ale pomaleji. Prusa: adresu najdete na displeji tiskárny v nastavení sítě. Klipper: Moonraker běží obvykle na portu 7125, ten se doplní sám; když ho máte jen přes port 80 (Mainsail, Fluidd), napište 192.168.1.50:80.",printUser:"Uživatel",printPass:"Heslo",printKey:"API klíč",printAuthHint:"Prusa chce jméno a heslo, nebo API klíč. Obojí najdete na displeji tiskárny v nastavení sítě u PrusaLinku, uživatel bývá maker. Klipper v domácí síti obvykle nic nechce; API klíč vyplňte, jen když ho máte v Moonrakeru zapnutý. Zařízení tiskárnu jen čte, nikdy ji neovládá. Adresa, jméno, heslo a klíč se ukládají až tlačítkem Uložit tiskárnu, aby se neposílaly při každém napsaném písmenu.",printExport:"Heslo a API klíč i do exportu a importu nastavení",printExportHint:"Heslo, jméno i klíč si zařízení pamatuje i po restartu, jeho paměť ale není šifrovaná. Zaškrtnuté se heslo a klíč přidají do souboru exportu nastavení (čitelně) a import je převezme; export je pak chráněný heslem správce, pokud ho máte. Bez zaškrtnutí v exportu nejsou a import je nemění.",printSave:"Uložit tiskárnu",printForget:"Zapomenout heslo a klíč",printNow:"Stav:",printIntro:"Ukazuje průběh tisku na obrazovce 3D tisk (zapnete na záložce Obrazovky) a upozorní, když tisk skončí, selže nebo se pozastaví. Obrazovka se do střídání přidává jen během tisku. Když tiskárna neodpovídá, zařízení se ptá jednou za minutu, po hodině jednou za 5 minut, a nic jiného tím nezdržuje.",printAlertHdr:"Upozornění",printAlertFinish:"Tisk je hotový",printAlertError:"Chyba tisku",printAlertPause:"Tisk se pozastavil (filament, tiskárna vyžaduje pozornost)",printNight:"V noci",printNight0:"noční jas, blikat",printNight1:"plný jas, blikat",printNight2:"noční jas, neblikat",printAlertHint:"Upozornění přepne na obrazovku 3D tisk, i když je vypnutá, a drží ji, dokud se displeje nedotknete. Když nikdo nepřijde, po 30 minutách se vrátí předchozí obrazovka a na hodinách zůstane barevná tečka. Zrušený tisk neupozorňuje, protože ho obvykle zrušil sám uživatel. Po restartu zařízení se už hotový tisk znovu nehlásí.",printSt:"3D tiskárna",printStored:"uloženo",printNone:"nezadáno",printInExport:"i v exportu",printForgotten:"Heslo a klíč zapomenuty",psDisabled:"vypnuto",psUnknown:"zjišťuji…",psOffline:"nedostupná",psAuth:"chybí nebo nesedí heslo / API klíč",psBad:"odpovídá, ale nerozumím (typ, adresa?)",psIdle:"připravena",psPrep:"připravuje se",psPrinting:"tiskne",psPaused:"pozastaveno",psFinished:"hotovo",psFailed:"chyba",psCancelled:"zrušeno",psLeft:"zbývá",psNext:"další pokus za"},
 en:{tabCtl:"Control",tabLoc:"Location",tabScr:"Screens",tabLook:"Display",tabClock:"Clock",tabMeteo:"Weather radar",fontHdr:"Typeface",tabPlanes:"Aircraft",
  tabEnergy:"Energy",tabSys:"System",scrPrice:"Electricity price",scrMix:"Czech generation",
  energyPrice:"Electricity price",energyMix:"Czech generation",mixCountry:"Country",
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
  wifiNow:"Connected to",autoSaved:"Saved",
  restartHint:"A change is saved straight away, but it takes effect only after a restart of the device. A bar with a Restart now button appears at the bottom; if you do not use it, the device restarts by itself 2 minutes after the last such change, so there is time to change several things at once.",
  rsTxt:"Changes take effect after a restart. The device restarts by itself in {t}.",rsNow:"Restart now",rsGoing:"Restarting, the page will reload in a moment…",
  remote:"Remote control",rangeLbl:"Range",statusHdr:"Device status",
  remoteHint:"The range only applies to the Aircraft and Weather screens. Using this pauses the automatic cycling.",
  location:"Location",findCity:"Find a town",search:"Search",found:"Found",lat:"Latitude",lon:"Longitude",
  screens:"Screens",
  scrClock:"Clock",scrPlanes:"Aircraft",scrMeteo:"Weather radar",scrForecast:"Forecast",board:"Board",
  scrHint:"Disabled screens are skipped and their tab on this page is hidden. Aircraft, Weather radar and Printer stay visible with the screen off while something that works without it is on: aircraft nearby, radar when a storm is coming, or the printer watch. The Settings screen on the display is always reachable.",
  autoRotate:"Auto cycling (seconds, 0 = off)",
  dotsHide:"Screen dots at the top",dotsAlways:"always shown",dots3:"hide after 3 s",dots5:"hide after 5 s",dots10:"hide after 10 s",
  dotsHint:"The dots show which screen you are on. They appear after a switch or a touch and go away after the chosen time. They always stay on the Settings screen, are not shown with an aircraft or print alert, and at night the auto cycling does not bring them up - only a touch, a long press or a switch from the browser does.",
  rotHint:"Cycling is paused by a swipe, a long press or a switch from the browser - for three times the interval, then it resumes on its own. A plain tap does not stop it; an open aircraft detail holds it. It does not run on the Settings screen.",
  radar:"Weather radar",radarSrc:"Data source",srcChmu:"CHMU (sharper, Czechia only)",
  srcRv:"RainViewer (Europe and beyond)",mtLegend:"Show the legend (dBZ / mm/h)",
  mtLegendHint:"The legend takes up the left edge of the map. If you know the scale, hide it and see more ground.",
  radarHint:"Outside Czechia CHMU has no data and the screen stays blank — use RainViewer.",
  brightness:"Brightness",clockHdr:"Clock",briDay:"Day brightness",briNight:"Night brightness",
  nightAuto:"Switch automatically with the sun",
  nightMode:"Night brightness",nmManual:"by hand (tap the clock)",nmSun:"with the sun",nmClock:"by the clock",
  nightWindow:"Night from – to",
  nightHint:"With the sun, night follows sunset and sunrise where the device is; the offset makes it longer or shorter. By the clock, night is the chosen window every day, and it may cross midnight (22:00 – 6:00).",
  darkHdr:"Display off at night",darkOn:"Switch the display off at night",darkWindow:"Off from – to",
  darkAlerts:"Alerts (aircraft, printer) light the display",
  darkHint:"In the chosen window the display goes fully dark, but the device keeps running and downloading. A touch lights it for 30 seconds; the first touch only lights it and switches nothing. With alerts ticked, a nearby aircraft or the printer lights it too; without, it stays dark, the aircraft alert is skipped and an unanswered printer alert leaves a coloured dot on the clock for the morning.",
  nameDay:"Show the name day above the date",
  clockLayout:"Clock layout",clA:"basic",clC:"with the Sun and the Moon",clD:"with the next hours",
  clockLayoutHint:"Basic: date, time, weather and wind spread over the display. With the Sun and the Moon: today's high and low in the weather row, and at the bottom the rise and set times of the Sun and the Moon; the Moon's icon shows its phase. With the next hours: the weather in 2, 4 and 6 hours at the bottom. In the last two the wind is in the weather row, and is left out when it does not fit - rain comes first. The Sun and the Moon are for the location in the settings; the Moon is worked out on the device.",
  nameDayHint:"Czech calendar in Czech, Slovak calendar in Slovak. Not shown in English, which has no name days. On a public holiday without a name the holiday is shown.",nightOffset:"Offset from sunrise/sunset (min)",
  secStyle:"Seconds ring",secOff:"off",secDots:"dots",secLine:"line",secComet:"comet",clockColor:"Clock colour",
  secColor:"Seconds colour",metric:"Metric units (m, km/h)",topBearing:"Window faces (top of the radar)",
  planes:"Aircraft",altMin:"Lowest altitude ({u})",altMax:"Highest altitude ({u})",
  onlyCs:"Only aircraft with a callsign",sqAlert:"Alert on emergency squawks (7500/7600/7700)",
  watch:"Watched callsign or ICAO",
  planesHint:"Filters affect drawing only. They never hide an emergency squawk or a watched aircraft.",
  planesView:"Radar view",
  showRoute:"Show the route (from – to) in the aircraft detail",
  showRouteHint:"The route is looked up by callsign in public databases (adsb.lol and adsbdb; when one is down the device switches to the other) and is sometimes wrong. Airlines use the same callsign on different legs and the database does not always keep up. Switched off, the route is neither shown nor looked up.",
  planesViewHint:"The radar is turned so that the direction you look out of the window is at the top. The window view below uses the same direction. The - and + buttons on the Settings screen change it too. The weather radar deliberately does not turn; a rain map is read with north up. Units apply to the aircraft detail.",
  pwNone:"No password is set yet - update, import and reset are open.",pwSet:"A password is set.",
  passNote:"Note: the default is no password, which leaves update, import and reset open to anyone on the network. The password is stored in the clear because the update page uses HTTP Basic, where a digest cannot be used - so it protects against the household, not against an attacker. A forgotten password can only be cleared by holding BOOT at startup (~3 s), which also erases WiFi and every other setting.",
  srcTitle:"Data sources",
  srcIntro:"The data comes from the services below. Several of them require their source to be credited, which is why they are listed here. The full terms are in LICENSE.txt.",
  system:"System",adminPass:"Admin password",newPass:"New password",
  passHint:"Only needed when a password is set. Firmware updates, settings import and the factory reset ask for it; saving the settings does not, and never changes it.",
  fwUpdate:"Firmware update",export:"Export settings",import:"Import",reboot:"Reboot",factory:"Factory reset",
  settings:"Settings",saved:"Saved",failed:"Failed",searching:"Searching…",
  nothing:"Nothing found",disabled:"That screen is switched off",
  confirmReset:"Really erase all settings including WiFi?",tabHa:"Home Assistant",wifiSaved:"Saved networks",
  wifiSavedHint:"The device remembers up to three networks. They can be typed in even when out of range - set up the office network while still at home. When the link drops, after a minute the device tries the other saved networks, strongest first. An empty password field keeps the stored password. Saving does not disconnect the device.",
  wifiSaveList:"Save networks",passStored:"password stored",passNone:"password",ssidPh:"network name",
  stormHdr:"Weather radar only for storms",
  stormOnly:"Weather radar in the cycling only when rain/a storm is coming",stormDbz:"Echo at least (dBZ)",
  stormKm:"Within (km)",
  stormHint:"The weather radar joins the cycling only when rain is coming. Every 5 minutes the newest CHMU radar frame is checked; when an echo at least this strong lies within this distance, the weather radar joins the cycling, and after two clear frames it drops out again. Works even with the Weather radar screen switched off above. Roughly: 20 dBZ light rain, 40 dBZ heavy rain, above 50 dBZ a storm with possible hail. The data is CHMU's, so Czechia and a little beyond. Needs automatic cycling on.",
  paHdr:"Aircraft nearby",planeAlert:"Show the aircraft only when one is near (or in the window view)",
  paKm:"Within (km)",paSec:"Show for (seconds)",
  paMode:"What to show",paMode0:"The aircraft detail at once",paMode1:"The radar first, then the detail",paMode2:"Only the radar with the aircraft marked",paModeHint:"On the radar the aircraft gets a cyan ring and the line under the clock says how far away it is. With the radar first, it is shown for about a quarter of the time set (at least 5 s, at most 15 s) and the detail gets the rest.",
  paHint:"When an aircraft comes close, the device switches to it for a moment. The Aircraft screen then never comes up in the cycling by itself: as soon as an aircraft comes this close, the device switches to it, shows that aircraft (the radar, its detail, or one after the other) and after the set time goes back to where it was. Each aircraft is shown once (per 15 minutes). Works without automatic cycling too. The window view (above) takes priority - an aircraft you can see from the window is shown even if it is further away than another one behind the house. Touching the screen during the alert ends it and stays on the aircraft.",
  srcChmuM:"CHMU masked (only rain reaching the ground)",
  srcDiff:"CHMU shows everything the radars see at full colour - including rain that evaporates before it reaches the ground and the anvils of distant storms. CHMU masked is the same picture, but at full colour only where measurements at 2 km say the rain actually reaches the ground; the rest is paler. For 'will it rain here?' masked is more accurate, for watching storms the full one is better.",
  clockFont:"Typeface (all screens)",fontThin:"Inter thin",fontPixel:"pixel (original)",
  altHdr:"Colours by altitude",altBelow:"below {a}",altB1:"Boundary 1 ({u})",altB2:"Boundary 2 ({u})",
  altB3:"Boundary 3 ({u})",altDef:"Default colours and boundaries",
  altHint:"Three boundaries split the altitudes into four bands, each with its own colour. The default is red below 2 km, orange 2-6 km, yellow 6-10 km and blue above 10 km. The boundaries must ascend. The boundaries and the legend on the display are in metres or in feet, following the Metric units option above.",
  altBad:"The boundaries must ascend",winHdr:"Window view",winOn:"Use the window view",
  dir0:"north (N)",dir45:"north-east (NE)",dir90:"east (E)",dir135:"south-east (SE)",dir180:"south (S)",
  dir225:"south-west (SW)",dir270:"west (W)",dir315:"north-west (NW)",winW:"Width of the view (°)",
  winKm:"Visibility (km)",
  winHint:"The slice of sky you see from the window. Which way the window faces is set above in Radar view; here only how wide an angle of sky you see from it (spread evenly around that direction - 90° on a north-east window covers north to east) and how far you can still make out an aircraft. It is drawn as thin lines on the aircraft radar. An aircraft in the view takes priority for the alert over a nearer one you cannot see.",
  haUntested:"This feature has not been tested with a real Home Assistant yet. The format follows the RESTful integration's documentation, but if something does not fit, please report it.",
  haOn:"Make the data available to Home Assistant",
  haHow:"Home Assistant fetches the data itself (about once a minute) from the address below. Nothing is sent anywhere and no MQTT is needed. The data is read-only: temperature, wind, rain, storm nearby, nearest aircraft, electricity price. While this is on, the device also watches for storms (one CHMU frame every 5 minutes) and nearby aircraft (a small request every 30 s).",
  haOpen:"open",haYamlHint:"Paste this into Home Assistant's configuration.yaml and restart Home Assistant:",
  haCopy:"Copy",copied:"Copied",stormSt:"Storm",planeWatchSt:"Aircraft nearby",srcNameDays:"name days",srcIcons:"aircraft icons after",
  iconHdr:"Aircraft icons",
  iconIntro:"The icon's shape follows the aircraft type and its size the weight. The type comes from the type code (A20N, B738, EC35...), or from the category the transponder sends when the type is unknown. The colour shows the altitude (see Colours by altitude above).",
  iconCredit:"The assignment of aircraft types to icons follows the scheme of ADS-B Radar (adsb-radar.com/help/icons.html). The shapes are drawn by the firmware itself; no icon files are used.",
  icNarrow:"narrow-body - A320, B737, B757",icWide:"wide-body twin - A330, A350, B767, B777, B787",
  icQuad:"four-engine - A340, A380, B747",icRegional:"regional jets - CRJ, Embraer E-Jet, Fokker 100",
  icBizjet:"business jets - Citation, Learjet, Falcon, Gulfstream",
  icTwinprop:"turboprops - ATR, Dash 8, King Air, L 410",
  icLight:"light aircraft - Cessna, Piper, Zlin, ultralights",
  icMiltrans:"military transport - C-130, C-160, A400M",
  icFreighter:"heavy freighters - An-124, C-17, C-5, Il-76",icFighter:"fighters and jet trainers",
  icGlider:"gliders and motor gliders",icTilt:"tiltrotors - V-22, AW609",icHeli:"helicopters",
  icBalloon:"balloons and airships",icDrone:"drones",icGeneric:"type not identified",
  icNoTrack:"direction of flight unknown",more:"More",less:"Less",pwChangeHdr:"Change the admin password",
  pwCur:"Current password",newPass2:"New password again",pwChange:"Change password",
  pwChangeHint:"Fill in the current password only if one is already set. An empty new password switches the protection off.",
  pwMismatch:"The new passwords do not match",
  pwOffConfirm:"An empty password switches off the protection of update, import and reset. Continue?",
  pwWrong:"Wrong admin password",pwChanged:"Password changed",pwRemoved:"Password protection is off",
  inclVat:" incl. VAT",altAbove:"above {a}",
  scrCrypto:"Crypto",crHdr:"Cryptocurrencies",crList:"Coins shown",crCount:"{n} selected, 6 at most.",crKey:"CoinGecko API key (optional)",crKeySave:"Save key",crKeyDel:"Delete",crKeyOn:"A key is saved (not shown, for safety).",crKeyOff:"No key - none is needed.",srcCrypto:"crypto",crSt:"Crypto",
  crIntro:"The Crypto screen shows the price of the chosen coins in the same home currency as the rates (top of this tab), with an arrow for the change over 24 hours. It is off by default; switch it on in the Screens tab.",
  crKeyHint:"Without a key CoinGecko allows about 5 requests a minute per IP address; the device asks once every 10 minutes. A (free Demo) key only helps when several devices share one IP. The key is not part of the settings export.",
  crSrcHint:"Source: CoinGecko, an average over exchanges - an indicative price, not what a particular trade would get. When CoinGecko does not answer, the device tries again after 2, 4, 8, 16 and then every 30 minutes; the time at the top turns orange when the data is not fresh, and after 6 hours the prices go grey too.",
  tabFx:"Rates & Crypto",scrFx:"Exchange rates",fxHdr:"Exchange rates",fxHome:"Home currency",fxAuto:"Automatic",fxList:"Currencies shown",fxDefault:"Default selection",fxCount:"{n} selected, 6 at most.",srcFx:"exchange rates",fxSt:"Exchange rates",
  fxIntro:"The Exchange rates screen shows what the chosen currencies cost in your home currency, with an arrow for the change against the previous fixing. It is off by default; switch it on in the Screens tab.",
  fxAutoHint:"Automatic means: Czech - koruna, Slovak - euro, English - the currency of the country chosen for the generation mix (euro otherwise).",
  fxSrcCnb:"Source: Czech National Bank, the daily exchange rate fixing. Published on working days around 14:30; at weekends and on holidays the last one stays valid.",
  fxSrcEcb:"Source: European Central Bank reference rates. Published on working days around 16:00; a rate between two currencies other than the euro is worked out through the euro.",
  fxListHint:"Six currencies at most; the home currency cannot be picked. Currencies with a small unit are shown per 100 or 1000 (e.g. 100 HUF), as on a bank's rate sheet. The home currency and the selection are part of the settings export and import.",
  routeSt:"Flight route",tabPrint:"Printer",scrPrint:"3D print",printHdr:"3D printer",printOn:"Watch a 3D printer",printType:"Type",printHost:"Address (IP)",printHostHint:"Enter the printer's IP address; a .local name works too, only slower. Prusa: the address is on the printer's display in the network settings. Klipper: Moonraker usually listens on port 7125, which is added by itself; if you only have it on port 80 (Mainsail, Fluidd), write 192.168.1.50:80.",printUser:"User",printPass:"Password",printKey:"API key",printAuthHint:"Prusa wants a user and password, or an API key. Both are on the printer's display in the network settings under PrusaLink; the user is usually maker. Klipper on a home network usually wants nothing; fill in an API key only if Moonraker has one switched on. The device only reads the printer, it never controls it. The address, user, password and key are saved with the Save printer button, so they are not sent with every letter typed.",printExport:"Include the password and API key in the settings export and import",printExportHint:"The device keeps the password, user and key across restarts, but its memory is not encrypted. Ticked, the password and key are added to the settings export file (readable) and an import takes them over; the export then asks for the admin password, if you have one. Unticked, they are not in the export and an import leaves them alone.",printSave:"Save printer",printForget:"Forget password and key",printNow:"State:",printIntro:"Shows the print on the 3D print screen (switch it on in the Screens tab) and alerts you when a print finishes, fails or pauses. The screen joins the cycling only while printing. When the printer does not answer, the device asks once a minute, after an hour once every 5 minutes, and nothing else waits for it.",printAlertHdr:"Alerts",printAlertFinish:"Print finished",printAlertError:"Print failed",printAlertPause:"Print paused (filament, printer needs attention)",printNight:"At night",printNight0:"night brightness, blink",printNight1:"full brightness, blink",printNight2:"night brightness, no blinking",printAlertHint:"An alert switches to the 3D print screen, even when it is switched off, and holds it until you touch the display. If nobody comes, after 30 minutes the previous screen returns and a coloured dot stays on the clock. A cancelled print gives no alert - it is nearly always the user's own doing. After a restart of the device a finished print is not reported again.",printSt:"3D printer",printStored:"stored",printNone:"not set",printInExport:"also in the export",printForgotten:"Password and key forgotten",psDisabled:"off",psUnknown:"checking…",psOffline:"not reachable",psAuth:"password / API key missing or wrong",psBad:"answers, but not as expected (type, address?)",psIdle:"ready",psPrep:"preparing",psPrinting:"printing",psPaused:"paused",psFinished:"finished",psFailed:"error",psCancelled:"cancelled",psLeft:"left",psNext:"next try in"},
 sk:{tabCtl:"Ovládanie",tabLoc:"Poloha",tabScr:"Obrazovky",tabLook:"Displej",tabClock:"Hodiny",tabMeteo:"Meteoradar",fontHdr:"Písmo",tabPlanes:"Lietadlá",
  tabEnergy:"Energia",tabSys:"Systém",scrPrice:"Cena elektriny",scrMix:"Výroba ČR",energyPrice:"Cena elektriny",
  energyMix:"Výroba ČR",mixCountry:"Krajina",
  mixCountryHint:"Rozhranie energy-charts pokrýva Európu. Mimo nej obrazovka dáta nemá a sama sa vypne.",
  priceCzOnly:"Ceny platia len pre Česko a sú v korunách. Keď nastavíte polohu mimo ČR, obrazovka sa sama vypne a zariadenie si o dáta prestane pýtať.",
  timezone:"Časové pásmo",
  priceIntro:"Obrazovka ukazuje burzovú (spotovú) cenu z denného trhu OTE ako dvadsaťštyrihodinový ciferník. Ceny na zajtra sa zverejňujú okolo 14:00, dovtedy sa dá zobraziť len dnešok.",
  priceFee:"Prirážka k burzovej cene (Kč/MWh)",priceVat:"DPH (%, 0 = nepripočítavať)",
  priceFeeHint:"V predvolenom stave sú obe polia nula a displej ukazuje čistú burzovú cenu — a napíše k nej „burza“, aby sa nedala zameniť s tým, čo platíte. Distribúciu, regulované zložky a maržu obchodníka si odpíšte z vyúčtovania; cenník ich zvyčajne uvádza v Kč/kWh, takže ich vynásobte tisícom. Zariadenie si nič nedomýšľa: pripočíta presne to, čo sem zadáte, a nič iné.",
  feeKwh:"Zodpovedá",
  mixHint:"Prstenec ukazuje, z čoho sa práve vyrába elektrina, v strede je podiel obnoviteľných zdrojov a dole saldo cezhraničných tokov. Dáta pochádzajú z ENTSO-E cez rozhranie energy-charts a zverejňujú sa s oneskorením jednej až dvoch hodín — čas vzorky je preto napísaný hore na obrazovke.",
  energySrcHint:"Oba zdroje sú zadarmo, bez kľúča a bez registrácie. Keď sú obe obrazovky vypnuté, zariadenie si o dáta vôbec nepýta.",
  wifi:"WiFi",network:"Sieť",password:"Heslo",scan:"Vyhľadať",connect:"Pripojiť",
  wifiHint:"Po uložení sa zariadenie pripojí a prístupový bod zmizne.",
  wifiHintSta:"Zmena siete preruší spojenie s touto stránkou. Keď sa pripojenie nepodarí, zariadenie vytvorí vlastnú sieť MeteoPlaneRadar a zadáte ju znova.",
  wifiNow:"Pripojené k sieti",autoSaved:"Uložené",
  restartHint:"Zmena sa uloží hneď, prejaví sa však až po reštarte zariadenia. Dole sa objaví pruh s tlačidlom Reštartovať teraz; keď ho nepoužijete, zariadenie sa reštartuje samo 2 minúty po poslednej takej zmene, takže stihnete zmeniť viac vecí naraz.",
  rsTxt:"Zmeny sa prejavia po reštarte. Zariadenie sa reštartuje samo o {t}.",rsNow:"Reštartovať teraz",rsGoing:"Reštartujem, stránka sa o chvíľu načíta znova…",
  remote:"Diaľkové ovládanie",rangeLbl:"Rozsah",statusHdr:"Stav zariadenia",
  remoteHint:"Rozsah sa mení len na obrazovkách Lietadlá a Meteoradar. Zásah pozastaví automatické striedanie.",
  location:"Poloha",findCity:"Nájsť mesto",search:"Hľadať",found:"Nájdené",lat:"Zemepisná šírka",
  lon:"Zemepisná dĺžka",
  screens:"Obrazovky",scrClock:"Hodiny",scrPlanes:"Lietadlá",scrMeteo:"Meteoradar",scrForecast:"Predpoveď",
  board:"Doska",scrHint:"Vypnuté obrazovky sa preskakujú a ich záložka tu na webe sa skryje. Lietadlá, Meteoradar a Tlačiareň zostanú viditeľné aj s vypnutou obrazovkou, keď je zapnuté to, čo funguje aj bez nej: lietadlo nablízku, meteoradar pri búrke alebo stráženie tlačiarne. Obrazovka Nastavenia na displeji je dostupná vždy.",
  autoRotate:"Automatické striedanie (sekundy, 0 = vypnuté)",
  dotsHide:"Bodky obrazoviek hore",dotsAlways:"vždy zobrazené",dots3:"skryť po 3 s",dots5:"skryť po 5 s",dots10:"skryť po 10 s",
  dotsHint:"Bodky ukazujú, na ktorej obrazovke ste. Po prepnutí alebo dotyku sa ukážu a po zvolenom čase zmiznú. Na obrazovke Nastavenia zostávajú vždy, pri upozornení na lietadlo alebo tlač sa neukazujú a v noci ich neprinesie automatické striedanie, len dotyk, dlhé stlačenie alebo prepnutie z prehliadača.",
  rotHint:"Striedanie pozastaví prejdenie prstom, dlhé stlačenie alebo prepnutie z prehliadača — na trojnásobok intervalu, potom pokračuje samo. Obyčajné ťuknutie ho nezastaví, otvorený detail lietadla ho drží. Na obrazovke Nastavenia sa nestrieda.",
  radar:"Meteoradar",radarSrc:"Zdroj dát",srcChmu:"ČHMÚ (ostrejší, len ČR)",srcRv:"RainViewer (Európa aj svet)",
  mtLegend:"Zobraziť legendu (dBZ / mm/h)",
  mtLegendHint:"Legenda zaberá ľavý okraj mapy. Keď stupnicu poznáte, dá sa skryť a je vidieť viac územia.",
  radarHint:"Mimo ČR nemá ČHMÚ dáta a obrazovka zostane prázdna — použite RainViewer. Slovenský radar SHMÚ zatiaľ nie je podporovaný.",
  brightness:"Jas",clockHdr:"Hodiny",briDay:"Denný jas",briNight:"Nočný jas",
  nightAuto:"Prepínať automaticky podľa slnka",
  nightMode:"Nočný jas zapínať",nmManual:"ručne (ťuknutím na hodiny)",nmSun:"podľa slnka",nmClock:"podľa času",
  nightWindow:"Noc od – do",
  nightHint:"Podľa slnka sa noc riadi západom a východom slnka v mieste zariadenia, posun ju predĺži alebo skráti. Podľa času platí noc každý deň vo zvolenom rozmedzí, môže aj cez polnoc (22:00 – 6:00).",
  darkHdr:"Vypnutie displeja v noci",darkOn:"Vypínať displej v noci",darkWindow:"Vypnuté od – do",
  darkAlerts:"Upozornenia (lietadlo, tlač) displej rozsvietia",
  darkHint:"Vo zvolenom čase displej úplne zhasne, zariadenie však beží ďalej a sťahuje dáta. Dotyk ho rozsvieti na 30 sekúnd, prvý dotyk len rozsvecuje a nič neprepne. So zaškrtnutými upozorneniami displej rozsvieti aj blízke lietadlo alebo tlačiareň; bez nich zostane tma, upozornenie na lietadlo sa vynechá a nevybavené upozornenie od tlačiarne ráno ukáže farebná bodka na hodinách.",
  nameDay:"Zobraziť meniny nad dátumom",
  clockLayout:"Rozloženie hodín",clA:"základné",clC:"so Slnkom a Mesiacom",clD:"s predpoveďou na ďalšie hodiny",
  clockLayoutHint:"Základné: dátum, čas, počasie a vietor rozložené po celom displeji. So Slnkom a Mesiacom: v riadku počasia dnešné max/min, dole východ a západ Slnka a Mesiaca; ikona Mesiaca ukazuje jeho fázu. S predpoveďou: dole počasie o 2, 4 a 6 hodín. Pri posledných dvoch je vietor v riadku s počasím, a keď sa tam nezmestí, vynechá sa – zrážky majú prednosť. Slnko aj Mesiac platia pre polohu zariadenia z nastavení; Mesiac sa počíta priamo v zariadení.",
  nameDayHint:"Po česky český kalendár, po slovensky slovenský. V angličtine sa meniny nezobrazujú. V štátny sviatok bez mena sa ukáže názov sviatku.",nightOffset:"Posun voči východu/západu (min)",
  secStyle:"Sekundový prstenec",secOff:"vypnuté",secDots:"bodky",secLine:"čiara",secComet:"kométa",
  clockColor:"Farba hodín",secColor:"Farba sekúnd",metric:"Metrické jednotky (m, km/h)",
  topBearing:"Okno smeruje na (hore na radare)",planes:"Lietadlá",altMin:"Najnižšia výška ({u})",
  altMax:"Najvyššia výška ({u})",onlyCs:"Len lietadlá s callsignom",
  sqAlert:"Upozorniť na núdzové squawky (7500/7600/7700)",watch:"Sledovaný callsign alebo ICAO",
  planesHint:"Filtre sa týkajú len kreslenia. Núdzový squawk ani sledované lietadlo neskryjú.",
  planesView:"Zobrazenie radaru",
  showRoute:"Zobraziť trasu (odkiaľ – kam) v detaile lietadla",
  showRouteHint:"Trasa sa dohľadáva podľa callsignu vo verejných databázach (adsb.lol a adsbdb; keď jedna nefunguje, zariadenie prejde na druhú) a občas nesedí. Letecké spoločnosti používajú rovnaký callsign na rôznych linkách a databáza to nemusí stihnúť. Vypnutá sa trasa nezobrazí ani nedohľadáva.",
  planesViewHint:"Radar sa natočí tak, aby smer, ktorým sa pozeráte z okna, bol hore. Rovnaký smer používa aj výhľad z okna nižšie. Mení sa aj tlačidlami − a + na obrazovke Nastavenia. Meteoradar sa zámerne neotáča, zrážková mapa sa číta severom hore. Jednotky platia pre detail lietadla.",
  pwNone:"Zatiaľ nie je nastavené žiadne heslo — aktualizácia, import a reset sú otvorené.",
  pwSet:"Heslo je nastavené.",
  passNote:"Poznámka: predvolený stav je bez hesla — aktualizácia, import aj reset sú potom prístupné komukoľvek v sieti. Heslo sa ukladá v čitateľnej podobe, pretože aktualizačná stránka používa HTTP Basic, kde odtlačok použiť nejde; chráni teda pred domácnosťou, nie pred útočníkom. Zabudnuté heslo sa dá zrušiť jedine podržaním tlačidla BOOT pri štarte (~3 s), čo zmaže aj WiFi a všetky ostatné nastavenia.",
  srcTitle:"Zdroje dát",
  srcIntro:"Dáta pochádzajú z týchto služieb. Niekoľko z nich vyžaduje uvedenie zdroja, preto sú uvedené tu. Podrobnosti a podmienky sú v súbore LICENSE.txt.",
  system:"Systém",adminPass:"Heslo správcu",newPass:"Nové heslo",
  passHint:"Vyplňte, len keď je heslo nastavené. Potrebuje ho aktualizácia firmvéru, import nastavení a továrenský reset; samotné ukladanie nastavení heslo nevyžaduje a nikdy ho nemení.",
  fwUpdate:"Aktualizácia firmvéru",export:"Export nastavení",import:"Import",reboot:"Reštart",
  factory:"Továrenský reset",settings:"Nastavenia",saved:"Uložené",
  failed:"Nepodarilo sa",searching:"Hľadám…",nothing:"Nič sa nenašlo",disabled:"Obrazovka je vypnutá",
  confirmReset:"Naozaj zmazať všetky nastavenia vrátane WiFi?",tabHa:"Home Assistant",wifiSaved:"Uložené siete",
  wifiSavedHint:"Zariadenie si pamätá až tri siete. Dajú sa zapísať ručne, aj keď práve nie sú v dosahu — napríklad sieť v kancelárii ešte doma. Po výpadku spojenia skúsi zariadenie po minúte ostatné uložené siete, najsilnejšiu ako prvú. Prázdne pole hesla nechá uložené heslo bez zmeny. Uloženie sietí zariadenie neodpojí.",
  wifiSaveList:"Uložiť siete",passStored:"heslo uložené",passNone:"heslo",ssidPh:"názov siete",
  stormHdr:"Meteoradar len pri búrke",stormOnly:"Meteoradar v striedaní len keď sa blížia zrážky/búrka",
  stormDbz:"Odraz aspoň (dBZ)",stormKm:"Do vzdialenosti (km)",
  stormHint:"Meteoradar sa do striedania pridá len vtedy, keď sa blížia zrážky. Každých 5 minút sa skontroluje najnovšia snímka radaru ČHMÚ; keď je v zadanej vzdialenosti odraz aspoň zadanej sily, meteoradar sa pridá do striedania a po dvoch pokojných snímkach zase vypadne. Funguje, aj keď je obrazovka Meteoradar vyššie vypnutá. Orientačne: 20 dBZ slabý dážď, 40 dBZ silný dážď, nad 50 dBZ búrka s možným krupobitím. Dáta sú len z ČHMÚ, takže v ČR a kúsok za hranicami. Potrebuje zapnuté automatické striedanie.",
  paHdr:"Lietadlo nablízku",planeAlert:"Ukázať lietadlá len keď je lietadlo blízko (alebo vo výhľade z okna)",
  paKm:"Do vzdialenosti (km)",paSec:"Ukázať na (sekundy)",
  paMode:"Čo ukázať",paMode0:"Hneď detail lietadla",paMode1:"Najprv radar, potom detail",paMode2:"Len radar s označeným lietadlom",paModeHint:"Na radare má lietadlo tyrkysový krúžok a pod hodinami je vidieť, ako je ďaleko. Pri voľbe „najprv radar“ sa radar ukáže približne na štvrtinu nastaveného času (najmenej 5 s, najviac 15 s) a zvyšok patrí detailu.",
  paHint:"Keď sa lietadlo priblíži, zariadenie naň na chvíľu prepne. Obrazovka Lietadlá sa potom sama neobjavuje v striedaní: hneď ako sa lietadlo priblíži na zadanú vzdialenosť, zariadenie na ňu prepne, ukáže to lietadlo (radar, detail alebo oboje po sebe) a po zadanom čase sa vráti tam, kde bolo. Každé lietadlo sa ukáže len raz (za 15 minút). Funguje aj bez automatického striedania. Výhľad z okna (vyššie) má prednosť — lietadlo, ktoré z okna vidíte, sa ukáže, aj keď je ďalej ako iné za domom. Dotyk počas ukážky ju ukončí a zostane sa na lietadlách.",
  srcChmuM:"ČHMÚ masked (len zrážky dopadajúce na zem)",
  srcDiff:"ČHMÚ ukazuje všetko, čo radary vidia, plnou farbou — vrátane zrážok, ktoré sa vyparia skôr, než dopadnú, a nákoví vzdialených búrok. ČHMÚ masked je tá istá snímka, ale plnou farbou len tam, kde podľa merania vo výške 2 km zrážky naozaj padajú na zem; zvyšok je bledší. Na otázku „bude u nás pršať?“ je masked presnejší, na sledovanie búrok je lepší plný.",
  clockFont:"Písmo (všetky obrazovky)",fontThin:"Inter tenké",fontPixel:"pixelové (pôvodné)",
  altHdr:"Farby podľa výšky",altBelow:"pod {a}",altB1:"1. hranica ({u})",altB2:"2. hranica ({u})",
  altB3:"3. hranica ({u})",altDef:"Predvolené farby a hranice",
  altHint:"Tri hranice rozdeľujú výšky do štyroch pásiem a každé pásmo má svoju farbu. Predvolená je červená pod 2 km, oranžová 2–6 km, žltá 6–10 km a modrá nad 10 km. Hranice musia ísť vzostupne. Hranice aj legenda na displeji sú v metroch, alebo v stopách podľa voľby Metrické jednotky vyššie.",
  altBad:"Hranice musia ísť vzostupne",winHdr:"Výhľad z okna",winOn:"Používať výhľad z okna",
  dir0:"sever (S)",dir45:"severovýchod (SV)",dir90:"východ (V)",dir135:"juhovýchod (JV)",dir180:"juh (J)",
  dir225:"juhozápad (JZ)",dir270:"západ (Z)",dir315:"severozápad (SZ)",winW:"Šírka výhľadu (°)",
  winKm:"Dohľad (km)",
  winHint:"Výsek oblohy, ktorý z okna vidíte. Kam okno smeruje, sa nastavuje vyššie v Zobrazení radaru; tu len aký široký uhol oblohy z okna vidíte (výsek je rozložený rovnomerne okolo smeru okna — 90° pri okne na SV pokryje sever až východ) a ako ďaleko lietadlo ešte rozoznáte. Na radare lietadiel sa zakreslí tenkými čiarami. Lietadlo vo výhľade má pri upozornení prednosť pred bližším lietadlom, ktoré z okna nevidno.",
  haUntested:"Táto funkcia zatiaľ nebola vyskúšaná so skutočným Home Assistantom. Formát zodpovedá dokumentácii integrácie RESTful, ale ak niečo nesedí, dajte vedieť.",
  haOn:"Sprístupniť dáta pre Home Assistant",
  haHow:"Home Assistant si dáta sám sťahuje (zhruba raz za minútu) z adresy nižšie. Nič sa nikam neposiela a nepotrebujete MQTT. Dáta sú len na čítanie: teplota, vietor, zrážky, búrka v okolí, najbližšie lietadlo, cena elektriny. Kým je to zapnuté, zariadenie stráži aj búrku (jedna snímka ČHMÚ za 5 minút) a lietadlá nablízku (malý dotaz každých 30 s).",
  haOpen:"otvoriť",
  haYamlHint:"Do súboru configuration.yaml v Home Assistante vložte toto a Home Assistant reštartujte:",
  haCopy:"Skopírovať",copied:"Skopírované",stormSt:"Búrka",planeWatchSt:"Lietadlá nablízku",
  srcNameDays:"meniny",srcIcons:"ikony lietadiel podľa",iconHdr:"Ikony lietadiel",
  iconIntro:"Tvar ikony na radare podľa typu lietadla, veľkosť zodpovedá hmotnosti. Typ sa spozná z kódu typu (A20N, B738, EC35…), pri neznámom z kategórie, ktorú vysiela transpondér. Farba ikony udáva výšku (pozri Farby podľa výšky vyššie).",
  iconCredit:"Rozdelenie typov lietadiel do ikon vychádza zo schémy ADS-B Radar (adsb-radar.com/help/icons.html). Tvary kreslí firmvér sám, súbory ikon sa nepreberajú.",
  icNarrow:"úzkotrupé — A320, B737, B757",icWide:"širokotrupé dvojmotorové — A330, A350, B767, B777, B787",
  icQuad:"štvormotorové — A340, A380, B747",icRegional:"regionálne — CRJ, Embraer E-Jet, Fokker 100",
  icBizjet:"business jet — Citation, Learjet, Falcon, Gulfstream",
  icTwinprop:"turbovrtuľové — ATR, Dash 8, King Air, L 410",
  icLight:"malé lietadlá — Cessna, Piper, Zlin, ultraľahké",
  icMiltrans:"vojenské transportné — C-130, C-160, A400M",
  icFreighter:"ťažké nákladné — An-124, C-17, C-5, Il-76",icFighter:"stíhačky a cvičné prúdové",
  icGlider:"vetrone a motorové vetrone",icTilt:"konvertoplány — V-22, AW609",icHeli:"vrtuľníky",
  icBalloon:"balóny a vzducholode",icDrone:"drony",icGeneric:"nezistený typ",icNoTrack:"neznámy smer letu",
  more:"Viac",less:"Menej",pwChangeHdr:"Zmeniť heslo správcu",pwCur:"Súčasné heslo",newPass2:"Nové heslo znova",
  pwChange:"Zmeniť heslo",
  pwChangeHint:"Súčasné heslo vyplňte, len keď už nejaké nastavené je. Prázdne nové heslo ochranu vypne.",
  pwMismatch:"Nové heslá sa nezhodujú",
  pwOffConfirm:"Prázdne heslo vypne ochranu aktualizácie, importu a resetu. Pokračovať?",
  pwWrong:"Nesprávne heslo správcu",pwChanged:"Heslo zmenené",pwRemoved:"Ochrana heslom je vypnutá",
  inclVat:" vrátane DPH",altAbove:"nad {a}",
  scrCrypto:"Kryptomeny",crHdr:"Kryptomeny",crList:"Zobrazované mince",crCount:"Vybrané {n} z najviac 6.",crKey:"API kľúč CoinGecko (nepovinný)",crKeySave:"Uložiť kľúč",crKeyDel:"Zmazať",crKeyOn:"Kľúč je uložený (z bezpečnosti sa nezobrazuje).",crKeyOff:"Bez kľúča – stačí to.",srcCrypto:"kryptomeny",crSt:"Kryptomeny",
  crIntro:"Obrazovka Kryptomeny ukazuje cenu vybraných mincí v rovnakej domácej mene ako kurzy (hore na tejto záložke) a šípkou zmenu za 24 hodín. V predvolenom stave je vypnutá, zapnete ju na záložke Obrazovky.",
  crKeyHint:"Bez kľúča CoinGecko povoľuje zhruba 5 dopytov za minútu na IP adresu, zariadenie sa pýta raz za 10 minút. Kľúč (bezplatný Demo) pomôže, len keď je za rovnakou IP viac zariadení. Kľúč sa do exportu nastavení neukladá.",
  crSrcHint:"Zdroj: CoinGecko, priemer z búrz – orientačná cena, nie cena konkrétneho nákupu. Keď CoinGecko neodpovedá, skúša to zariadenie znova o 2, 4, 8, 16 a potom každých 30 minút; čas hore zoranžovie, keď dáta nie sú čerstvé, a po 6 hodinách zošednú aj ceny.",
  tabFx:"Kurzy & Krypto",scrFx:"Kurzy mien",fxHdr:"Kurzy mien",fxHome:"Domáca mena",fxAuto:"Automaticky",fxList:"Zobrazované meny",fxDefault:"Predvolený výber",fxCount:"Vybrané {n} z najviac 6.",srcFx:"kurzy",fxSt:"Kurzy mien",
  fxIntro:"Obrazovka Kurzy mien ukazuje, koľko stoja vybrané meny v domácej mene, a šípkou zmenu oproti predchádzajúcemu kurzu. V predvolenom stave je vypnutá, zapnete ju na záložke Obrazovky.",
  fxAutoHint:"Automaticky znamená: čeština – koruna, slovenčina – euro, angličtina – mena krajiny vybranej pri výrobe elektriny (inak euro).",
  fxSrcCnb:"Zdroj: Česká národná banka, kurzy devízového trhu. Vyhlasujú sa v pracovné dni okolo 14:30, cez víkend a vo sviatok platí posledný kurz.",
  fxSrcEcb:"Zdroj: Európska centrálna banka, referenčné kurzy. Vyhlasujú sa v pracovné dni okolo 16:00; kurz medzi dvoma menami inými než euro sa prepočíta cez euro.",
  fxListHint:"Najviac šesť mien, domácu menu vybrať nejde. Meny s malou hodnotou jednotky sa ukazujú za 100 alebo 1000 kusov (napríklad 100 HUF), rovnako ako na kurzovom lístku. Domáca mena aj výber sú súčasťou exportu a importu nastavení.",
  routeSt:"Trasa letu",tabPrint:"Tlačiareň",scrPrint:"3D tlač",printHdr:"3D tlačiareň",printOn:"Strážiť 3D tlačiareň",printType:"Typ",printHost:"Adresa (IP)",printHostHint:"Zadajte IP adresu tlačiarne, názov s .local funguje tiež, ale pomalšie. Prusa: adresu nájdete na displeji tlačiarne v nastaveniach siete. Klipper: Moonraker beží zvyčajne na porte 7125, ten sa doplní sám; keď ho máte len cez port 80 (Mainsail, Fluidd), napíšte 192.168.1.50:80.",printUser:"Používateľ",printPass:"Heslo",printKey:"API kľúč",printAuthHint:"Prusa chce meno a heslo, alebo API kľúč. Oboje nájdete na displeji tlačiarne v nastaveniach siete pri PrusaLinku, používateľ býva maker. Klipper v domácej sieti zvyčajne nič nechce; API kľúč vyplňte, len keď ho máte v Moonrakeri zapnutý. Zariadenie tlačiareň len číta, nikdy ju neovláda. Adresa, meno, heslo a kľúč sa ukladajú až tlačidlom Uložiť tlačiareň, aby sa neposielali pri každom napísanom písmene.",printExport:"Heslo a API kľúč aj do exportu a importu nastavení",printExportHint:"Heslo, meno aj kľúč si zariadenie pamätá aj po reštarte, jeho pamäť však nie je šifrovaná. Zaškrtnuté sa heslo a kľúč pridajú do súboru exportu nastavení (čitateľne) a import ich prevezme; export je potom chránený heslom správcu, ak ho máte. Bez zaškrtnutia v exporte nie sú a import ich nemení.",printSave:"Uložiť tlačiareň",printForget:"Zabudnúť heslo a kľúč",printNow:"Stav:",printIntro:"Ukazuje priebeh tlače na obrazovke 3D tlač (zapnete na záložke Obrazovky) a upozorní, keď tlač skončí, zlyhá alebo sa pozastaví. Obrazovka sa do striedania pridáva len počas tlače. Keď tlačiareň neodpovedá, zariadenie sa pýta raz za minútu, po hodine raz za 5 minút, a nič iné tým nezdržuje.",printAlertHdr:"Upozornenia",printAlertFinish:"Tlač je hotová",printAlertError:"Chyba tlače",printAlertPause:"Tlač sa pozastavila (filament, tlačiareň vyžaduje pozornosť)",printNight:"V noci",printNight0:"nočný jas, blikať",printNight1:"plný jas, blikať",printNight2:"nočný jas, neblikať",printAlertHint:"Upozornenie prepne na obrazovku 3D tlač, aj keď je vypnutá, a drží ju, kým sa displeja nedotknete. Keď nikto nepríde, po 30 minútach sa vráti predchádzajúca obrazovka a na hodinách zostane farebná bodka. Zrušená tlač neupozorňuje, pretože ju zvyčajne zrušil sám používateľ. Po reštarte zariadenia sa už hotová tlač znova nehlási.",printSt:"3D tlačiareň",printStored:"uložené",printNone:"nezadané",printInExport:"aj v exporte",printForgotten:"Heslo a kľúč zabudnuté",psDisabled:"vypnuté",psUnknown:"zisťujem…",psOffline:"nedostupná",psAuth:"chýba alebo nesedí heslo / API kľúč",psBad:"odpovedá, ale nerozumiem (typ, adresa?)",psIdle:"pripravená",psPrep:"pripravuje sa",psPrinting:"tlačí",psPaused:"pozastavené",psFinished:"hotovo",psFailed:"chyba",psCancelled:"zrušené",psLeft:"zostáva",psNext:"ďalší pokus o"}};
let L="cs", CFG={}, TAB="tCtl";
const $=id=>document.getElementById(id);

// 0 = Czech, 1 = English, 2 = Slovak - the same numbers the firmware uses.
const LANGS=["cs","en","sk"];
function setLang(v){L=LANGS[+v]||(LANGS.includes(v)?v:"cs");$("uiLang").value=String(LANGS.indexOf(L));
 document.documentElement.lang=L;
 document.querySelectorAll("[data-i18n]").forEach(e=>{const t=D[L][e.dataset.i18n];if(!t)return;
  if(e.classList.contains("hint"))hint(e,t);else e.textContent=t.replace("{u}",MET()?"m":"ft");});
 pwState();feeHint();status();if(CFG.wifi)wifiSlots();iconLegend();altLabels();if(CFG.printUser!==undefined)printTypeUi();if(CFG.fxListEff)fxFill();}

// A hint shows its first sentence; the rest is folded under "More", which
// reads "Less" while it is open (0.9.0). Splitting
// here rather than in the dictionary keeps every text in one piece per language.
function hint(el,t){
 const m=t.match(/^(.+?[.!?])\s+(?=[A-ZÁČĎÉĚÍĽĹŇÓÔŔŘŠŤÚŮÝŽ0-9„"(])/);
 el.textContent="";
 if(!m){el.textContent=t;return;}
 el.appendChild(document.createTextNode(m[1]));
 const d=document.createElement("details"),sm=document.createElement("summary");
 sm.textContent=D[L].more;d.appendChild(sm);d.appendChild(document.createTextNode(" "+t.slice(m[0].length)));
 d.addEventListener("toggle",()=>{sm.textContent=d.open?D[L].less:D[L].more;});
 el.appendChild(d);
}

// Settings that only matter with a feature switched on are hidden while it is off.
// 0.9.2: times as "HH:MM" on the page, minutes of the day in the firmware.
function tMin(v){const m=/^(\d{1,2}):(\d{2})/.exec(v||"");return m?(+m[1]*60+ +m[2])%1440:0;}
function tStr(n){n=((+n||0)%1440+1440)%1440;return String(Math.floor(n/60)).padStart(2,"0")+":"+String(n%60).padStart(2,"0");}
// Night by: 0 by hand, 1 the sun, 2 the clock = nightAuto + nightSrc.
function nmVal(){return !CFG.nightAuto?0:(CFG.nightSrc===1?2:1);}
function nightModeUi(){const v=$("nightMode").value;$("nmSunRow").classList.toggle("hide",v!=="1");$("nmClockRow").classList.toggle("hide",v!=="2");}
$("nightMode").addEventListener("change",()=>{const v=$("nightMode").value;nightModeUi();
 CFG.nightAuto=v!=="0";autoSave("nightAuto",CFG.nightAuto);
 if(v!=="0"){CFG.nightSrc=v==="2"?1:0;autoSave("nightSrc",CFG.nightSrc);}});
function deps(){document.querySelectorAll(".dep").forEach(d=>{d.classList.toggle("hide",!$(d.dataset.dep).checked);});}
document.querySelectorAll(".dep").forEach(d=>$(d.dataset.dep).addEventListener("change",deps));

// Tabs. 0.9.8: no Save button on any of them - see autoSave() and rsBar().
function showTab(id){
 TAB=id;tabVis();
 document.querySelectorAll("section.tab").forEach(s=>s.classList.toggle("hide",s.id!=id));
 document.querySelectorAll("#tabs button").forEach(b=>b.classList.toggle("on",b.dataset.tab==id));
 window.scrollTo(0,0);
}
document.querySelectorAll("#tabs button").forEach(b=>b.onclick=()=>showTab(b.dataset.tab));

// 0.9.8: a screen's tab is there only while the screen is switched on (the
// checkbox, saved or not). Three features work without their screen - the
// aircraft-nearby alert, the radar when a storm is coming and the printer
// watch - and keep their tab while they are on, or there would be no way to
// switch them off. The tab being looked at stays until you leave it, so
// unticking something on it does not throw you out mid-edit.
const isOn=id=>$(id).checked;
const TAB_VIS={
 tClock:()=>isOn("sClock"),
 tPlanes:()=>isOn("sPlanes")||isOn("planeAlert"),
 tMeteo:()=>isOn("sMeteo")||isOn("stormOnly"),
 tEnergy:()=>isOn("sPrice")||isOn("sMix"),
 tPrint:()=>isOn("sPrint")||isOn("printOn"),
 tFx:()=>isOn("sFx")||isOn("sCrypto")};
function tabVis(){
 for(const id in TAB_VIS){
  const b=document.querySelector('#tabs button[data-tab="'+id+'"]');
  b.classList.toggle("hide",!TAB_VIS[id]()&&TAB!=id);
 }
 $("cardPrice").classList.toggle("hide",!isOn("sPrice"));
 $("cardMix").classList.toggle("hide",!isOn("sMix"));
 $("cardCrypto").classList.toggle("hide",!isOn("sCrypto"));
}
["sClock","sPlanes","sMeteo","sPrice","sMix","sPrint","sFx","sCrypto","planeAlert","stormOnly","printOn"]
 .forEach(id=>$(id).addEventListener("change",tabVis));

// One timer: an older message's timeout must not wipe a newer message early.
let msgT=0;
function msg(t,c){$("msg").textContent=t;$("msg").className=c||"";clearTimeout(msgT);msgT=setTimeout(()=>{$("msg").textContent=""},4000);}
function rgb565ToHex(v){const r=(v>>11&31)*255/31|0,g=(v>>5&63)*255/63|0,b=(v&31)*255/31|0;
 return "#"+[r,g,b].map(x=>x.toString(16).padStart(2,"0")).join("");}
function hexToRgb565(h){const r=parseInt(h.substr(1,2),16),g=parseInt(h.substr(3,2),16),b=parseInt(h.substr(5,2),16);
 return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);}

const SCR=[["scrClock",0],["scrPlanes",1],["scrMeteo",2],["scrForecast",3],
           ["scrPrice",4],["scrMix",5],["scrPrint",7],["scrFx",8],["scrCrypto",9],["settings",6]];
function drawScrBtns(cur,enabled){
 $("scrBtns").innerHTML=SCR.map(([k,i])=>{
  const on=enabled?enabled[i]:true;
  const cls=(i==cur)?"":"sec";
  const dis=on?"":" disabled style='opacity:.4'";
  return "<button class='"+cls+"'"+dis+" onclick='goScreen("+i+")'>"+D[L][k]+"</button>";
 }).join(" ");
}
// POST JSON. A 403 from a protected action means the admin password did not
// match - said in words rather than as a bare "Failed".
async function api(u,b){
 const r=await fetch(u,{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(b||{})});
 if(r.status==403)msg(D[L].pwWrong,"err");
 return r;
}
async function post(u,b){try{const r=await api(u,b);
 if(r.status==409)msg(D[L].disabled,"warn");else if(!r.ok&&r.status!=403)msg(D[L].failed,"err");
 await status();}catch(e){msg(D[L].failed,"err")}}
function goScreen(i){post("/api/screen",{index:i});}
function stepScreen(d){post("/api/screen",{step:d});}
function stepRange(d){post("/api/range",{step:d});}

// --- Okamzite ukladani ------------------------------------------------------
// The API applies only the keys it actually receives, so a single field can be
// sent on its own without disturbing anything else.
//
// 0.9.8: every field, including the three that need a restart (the location,
// the set of screens, the radar source). The device holds those back and says
// so in its answer; the page then shows the restart bar. Nothing restarts
// mid-keystroke: fields are sent on "change", and the device waits 2 minutes
// after the last such change before restarting by itself.
let saveTimer = {};
function autoSave(key, val){const o={};o[key]=val;autoSaveObj(key,o);}
function autoSaveObj(key, o){
 clearTimeout(saveTimer[key]);
 saveTimer[key] = setTimeout(async () => {
  try{
   const r = await fetch("/api/config",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(o)});
   msg(r.ok ? D[L].autoSaved : D[L].failed, r.ok ? "ok" : "err");
   if(r.ok){try{rsBar(await r.json());}catch(e){}}
  }catch(e){ msg(D[L].failed,"err"); }
 }, 350);
}

// The restart bar (0.9.8). Shown on every tab while the device holds changes
// that need a restart, with the device's own countdown; "Restart now" writes
// them and restarts at once.
let RS_LEFT=0,RS_GOING=false;
function rsDraw(){
 const on=RS_GOING||RS_LEFT>0;
 $("rsBar").classList.toggle("hide",!on);document.body.classList.toggle("hasbar",on);
 $("rsBar").querySelector("button").classList.toggle("hide",RS_GOING);
 if(RS_GOING){$("rsTxt").textContent=D[L].rsGoing;return;}
 if(on)$("rsTxt").textContent=D[L].rsTxt.replace("{t}",Math.floor(RS_LEFT/60)+":"+String(RS_LEFT%60).padStart(2,"0"));
}
function rsBar(j){if(!j||j.restartIn===undefined||RS_GOING)return;RS_LEFT=+j.restartIn||0;rsDraw();}
setInterval(()=>{if(RS_GOING)return;if(RS_LEFT>0){RS_LEFT--;if(RS_LEFT==0)rsWait();else rsDraw();}},1000);
// The device is going down: wait for it to answer again, then reload.
function rsWait(){
 RS_GOING=true;rsDraw();
 const t0=Date.now();
 const poll=async()=>{try{const r=await fetch("/api/status",{cache:"no-store"});if(r.ok&&Date.now()-t0>4000){location.reload();return;}}catch(e){}
  setTimeout(poll,1500);};
 setTimeout(poll,3000);
}
async function restartNow(){try{await fetch("/api/reboot",{method:"POST"});}catch(e){}rsWait();}

// The location goes as a pair; the screens as one set.
function saveLoc(){const la=parseFloat($("lat").value),lo=parseFloat($("lon").value);
 if(isNaN(la)||isNaN(lo))return;autoSaveObj("loc",{lat:la,lon:lo});}
["lat","lon"].forEach(id=>$(id).addEventListener("change",saveLoc));
const SCR_IDS={clock:"sClock",planes:"sPlanes",meteo:"sMeteo",forecast:"sForecast",price:"sPrice",mix:"sMix",print:"sPrint",fx:"sFx",crypto:"sCrypto"};
function saveScreens(){const o={};for(const k in SCR_IDS)o[k]=$(SCR_IDS[k]).checked;autoSaveObj("screens",{screens:o});}
Object.values(SCR_IDS).forEach(id=>$(id).addEventListener("change",saveScreens));

// [id, event, JSON key, how to read the value]
const AUTO = [
 ["uiLang","change","lang",e=>+e.value],
 ["metric","change","metric",e=>e.checked],
 ["topBearing","change","topBearing",e=>+e.value],
 ["briDay","input","briDay",e=>+e.value],
 ["briNight","input","briNight",e=>+e.value],
 ["nightFrom","change","nightFrom",e=>tMin(e.value)],
 ["nightTo","change","nightTo",e=>tMin(e.value)],
 ["darkOn","change","darkOn",e=>e.checked],
 ["darkFrom","change","darkFrom",e=>tMin(e.value)],
 ["darkTo","change","darkTo",e=>tMin(e.value)],
 ["darkAlerts","change","darkAlerts",e=>e.checked],
 ["nameDay","change","nameDay",e=>e.checked],
 ["clockLayout","change","clockLayout",e=>+e.value],
 ["nightOffset","change","nightOffset",e=>+e.value],
 ["secStyle","change","secStyle",e=>+e.value],
 ["clockColor","change","clockColor",e=>hexToRgb565(e.value)],
 ["secColor","change","secColor",e=>hexToRgb565(e.value)],
 ["altMin","change","altMin",e=>fOut(+e.value)],
 ["altMax","change","altMax",e=>fOut(+e.value)],
 ["onlyCallsign","change","onlyCallsign",e=>e.checked],
 ["squawkAlert","change","squawkAlert",e=>e.checked],
 ["meteoLegend","change","meteoLegend",e=>e.checked],
 ["radarSrc","change","radarSrc",e=>+e.value],
 ["showRoute","change","showRoute",e=>e.checked],
 ["watch","change","watch",e=>e.value],
 ["autoRotate","change","autoRotate",e=>+e.value],
 ["dotsHide","change","dotsHide",e=>+e.value],
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
 ["planeAlertMode","change","planeAlertMode",e=>+e.value],
 ["windowOn","change","windowOn",e=>e.checked],
 ["windowWidth","change","windowWidth",e=>+e.value],
 ["windowKm","change","windowKm",e=>+e.value],
 ["haEnabled","change","haEnabled",e=>e.checked],
 ["printOn","change","printOn",e=>e.checked],
 ["printType","change","printType",e=>+e.value],
 ["printAlertFinish","change","printAlertFinish",e=>e.checked],
 ["printAlertError","change","printAlertError",e=>e.checked],
 ["printAlertPause","change","printAlertPause",e=>e.checked],
 ["printNight","change","printNight",e=>+e.value],
];
// The altitude levels and the colours each go as one set - applied one value
// at a time, the levels would pass through an out-of-order state and be refused.
["ab0","ab1","ab2"].forEach(id=>{$(id).addEventListener("change",()=>altSave("bands"));$(id).addEventListener("input",altLabels);});
["ac0","ac1","ac2","ac3"].forEach(id=>$(id).addEventListener("change",()=>altSave("colors")));
// 0.8.7: altitudes follow the units option. The firmware keeps the colour
// boundaries in metres and the filter in feet; the page shows both in metres
// with "Metric units" on and in feet with it off, and converts on the way in
// and out. Feet are rounded to 100 ft and metres to 100 m (filter) or 1 m
// (boundaries), so a value typed in either unit reads back the same.
function MET(){return $("metric").checked;}
const bandIn=m=>MET()?m:Math.round(m/0.3048/100)*100;          // stored m -> field
const bandOut=v=>MET()?Math.round(v):Math.round(v*0.3048);     // field -> stored m
const fIn=ft=>MET()?Math.round(ft*0.3048/100)*100:ft;          // stored ft -> field
const fOut=v=>Math.min(60000,Math.max(0,MET()?Math.round(v/0.3048):Math.round(v)));
let BANDS_M=[2000,6000,10000],FILT_FT=[0,60000];
function unitFields(){
 BANDS_M.forEach((m,i)=>{const e=$("ab"+i);e.value=bandIn(m);e.step=MET()?100:500;e.max=MET()?20000:65000;});
 ["altMin","altMax"].forEach((id,i)=>{const e=$(id);e.value=fIn(FILT_FT[i]);e.step=MET()?100:500;e.max=MET()?18300:60000;});
 document.querySelectorAll("[data-i18n=altB1],[data-i18n=altB2],[data-i18n=altB3],[data-i18n=altMin],[data-i18n=altMax]")
  .forEach(e=>e.textContent=D[L][e.dataset.i18n].replace("{u}",MET()?"m":"ft"));
 altLabels();
}
$("metric").addEventListener("change",unitFields);
["altMin","altMax"].forEach((id,i)=>$(id).addEventListener("change",e=>{FILT_FT[i]=fOut(+e.target.value);}));
// A field still showing what was loaded keeps the exact stored metres - so
// changing one boundary in feet does not nudge the other two by rounding.
function altBands(){return[0,1,2].map(i=>{const v=+$("ab"+i).value;return v===bandIn(BANDS_M[i])?BANDS_M[i]:bandOut(v);});}
function altColors(){return[0,1,2,3].map(i=>hexToRgb565($("ac"+i).value));}
function altOk(b){return b[0]>0&&b[0]<b[1]&&b[1]<b[2];}
// The colour rows say which altitudes they cover, from the boundaries above:
// "below 2 km", "2-6 km", ... "above 10 km".
function km(v){const t=(Math.round(v/100)/10).toString();return L=="en"?t:t.replace(".",",");}
// Feet to the nearest 100, grouped by thousands: 6 600 (cs, sk), 6,600 (en).
function ftx(m){return (Math.round(m/0.3048/100)*100).toLocaleString(L=="en"?"en-US":"cs-CZ");}
function altLabels(){
 const b=altBands(),T=D[L],f=MET()?km:ftx,u=MET()?" km":" ft";
 const lab=[T.altBelow.replace("{a}",f(b[0])+u),f(b[0])+"–"+f(b[1])+u,
            f(b[1])+"–"+f(b[2])+u,T.altAbove.replace("{a}",f(b[2])+u)];
 lab.forEach((t,i)=>{if($("al"+i))$("al"+i).textContent=t;});
}
function altSave(what){
 if(what=="bands"){const b=altBands();if(!altOk(b)){msg(D[L].altBad,"warn");return;}BANDS_M=b;autoSave("altBands",b);}
 else autoSave("altColors",altColors());
}
function altDefaults(){
 BANDS_M=[2000,6000,10000];BANDS_M.forEach((v,i)=>$("ab"+i).value=bandIn(v));
 [0xF800,0xFC00,0xFFE0,0x001F].forEach((v,i)=>$("ac"+i).value=rgb565ToHex(v));
 autoSave("altBands",BANDS_M);autoSave("altColors",altColors());altLabels();
}
let wired=false;   // load() runs again after an import - wire only once
function wireAutoSave(){
 if(wired)return;wired=true;
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
 $("sPrint").checked=!!CFG.screens.print;$("sFx").checked=!!CFG.screens.fx;$("sCrypto").checked=!!CFG.screens.crypto;
 fxFill();
 printFill();
 $("priceFee").value=CFG.priceFee||0;$("priceVat").value=CFG.priceVat||0;
 $("mixCountry").value=CFG.mixCountry||"cz";
 feeHint();
 $("autoRotate").value=CFG.autoRotate;$("dotsHide").value=String(CFG.dotsHide||0);$("radarSrc").value=CFG.radarSrc;
 $("meteoLegend").checked=CFG.meteoLegend;$("showRoute").checked=CFG.showRoute!==false;
 $("briDay").value=CFG.briDay;$("briNight").value=CFG.briNight;
 $("nightMode").value=String(nmVal());$("nightOffset").value=CFG.nightOffset;
 $("nightFrom").value=tStr(CFG.nightFrom??1320);$("nightTo").value=tStr(CFG.nightTo??360);
 $("darkOn").checked=!!CFG.darkOn;$("darkFrom").value=tStr(CFG.darkFrom??1380);$("darkTo").value=tStr(CFG.darkTo??360);
 $("darkAlerts").checked=CFG.darkAlerts!==false;$("nameDay").checked=CFG.nameDay!==false;$("clockLayout").value=String(CFG.clockLayout||0);nightModeUi();
 $("secStyle").value=CFG.secStyle;$("metric").checked=CFG.metric;
 // Eight directions; anything between them snaps to the nearest one.
 $("topBearing").value=String((Math.round((CFG.topBearing||0)/45)%8)*45);
 $("clockColor").value=rgb565ToHex(CFG.clockColor);$("secColor").value=rgb565ToHex(CFG.secColor);
 FILT_FT=[CFG.altMin,CFG.altMax];
 pwState();
 $("onlyCallsign").checked=CFG.onlyCallsign;$("squawkAlert").checked=CFG.squawkAlert;$("watch").value=CFG.watch||"";
 $("clockFont").value=(CFG.clockFont!=null)?CFG.clockFont:1;
 BANDS_M=(CFG.altBands||[2000,6000,10000]).slice();
 (CFG.altColors||[0xF800,0xFC00,0xFFE0,0x001F]).forEach((v,i)=>$("ac"+i).value=rgb565ToHex(v));
 $("stormOnly").checked=!!CFG.stormOnly;$("stormDbz").value=CFG.stormDbz||20;$("stormKm").value=CFG.stormKm||50;
 $("planeAlert").checked=!!CFG.planeAlert;$("planeAlertKm").value=CFG.planeAlertKm||5;$("planeAlertSec").value=CFG.planeAlertSec||15;$("planeAlertMode").value=CFG.planeAlertMode||0;
 $("windowOn").checked=!!CFG.windowOn;
 $("windowWidth").value=CFG.windowWidth||90;$("windowKm").value=CFG.windowKm||15;
 $("haEnabled").checked=!!CFG.haEnabled;haInfo();
 wifiSlots();
 if(CFG.apMode)$("wifiSlots").classList.add("hide");
 unitFields();deps();bri();wireAutoSave();status();tabVis();rsBar(CFG);
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
 const num=kwh.toFixed(2).replace(".",L=="en"?".":",");
 const unit=(L=="en")?" CZK/kWh":" Kč/kWh";
 const vatNote=v?D[L].inclVat:"";
 $("feeKwh").textContent=D[L].feeKwh+" "+num+unit+vatNote;
}
$("priceFee").addEventListener("input",feeHint);
$("priceVat").addEventListener("input",feeHint);

async function status(){
 try{const s=await(await fetch("/api/status")).json();
 drawScrBtns(s.screen,s.enabled);
 rsBar(s);
 if(!CFG.apMode&&$("wifiNow")) $("wifiNow").textContent=D[L].wifiNow+" "+s.ssid+".";
 const hasR=(s.range&&s.range.length>0);
 $("rangeNow").textContent=hasR?s.range:"–";
 $("rMinus").disabled=!hasR;$("rPlus").disabled=!hasR;
 $("rMinus").style.opacity=$("rPlus").style.opacity=hasR?"1":".4";
 const rows=[["IP",s.ip],["WiFi",s.ssid+" ("+s.rssi+" dBm)"],["Uptime",s.uptime],
  ["Heap",s.heap+" B"],["PSRAM",s.psram+" B"],["Restart",s.resetReason],
  ["ADS-B",s.adsb],["Radar",s.radar],[D[L].scrForecast,s.forecast],
  [D[L].scrPrice,s.price],[D[L].scrMix,s.mix],
  [D[L].stormSt,s.storm],[D[L].planeWatchSt,s.planeWatch],[D[L].printSt,s.printWatch],[D[L].fxSt,s.fx],[D[L].crSt,s.crypto],[D[L].routeSt,s.route],
  [D[L].timezone,s.timezone],
  ["Firmware","v"+s.version],[D[L].board,s.board]];
 printStatus(s.print);
 $("statusTab").innerHTML=rows.map(r=>"<tr><td>"+r[0]+"</td><td>"+r[1]+"</td></tr>").join("");}catch(e){}
}
setInterval(status,10000);

// --- 3D printer (0.9.0) ---------------------------------------------------------
// The secrets never come back from the device: the fields stay empty and a
// line says whether one is stored. Empty fields on save leave them unchanged.
function printFill(){
 $("printOn").checked=!!CFG.printOn;$("printType").value=String(CFG.printType||0);
 $("printHost").value=CFG.printHost||"";$("printUser").value=CFG.printUser||"maker";
 $("printExport").checked=!!CFG.printExport;
 $("printAlertFinish").checked=CFG.printAlertFinish!==false;
 $("printAlertError").checked=CFG.printAlertError!==false;
 $("printAlertPause").checked=CFG.printAlertPause!==false;
 $("printNight").value=String(CFG.printNight||0);
 $("printPass").value="";$("printKey").value="";
 printTypeUi();
}
function printTypeUi(){
 const prusa=$("printType").value=="0";
 document.querySelectorAll(".prusaOnly").forEach(e=>e.classList.toggle("hide",!prusa));
 const T=D[L],parts=[];
 if(prusa)parts.push(T.printPass+": "+(CFG.printHasPass?T.printStored:T.printNone));
 parts.push(T.printKey+": "+(CFG.printHasKey?T.printStored:T.printNone));
 $("printSecretState").textContent=parts.join(" · ")+(CFG.printExport?" ("+T.printInExport+")":"");
}
$("printType").addEventListener("change",printTypeUi);
// The Home Assistant YAML lists the print sensors only with the watch on.
$("printOn").addEventListener("change",()=>{CFG.printOn=$("printOn").checked;haInfo();});
function printStatus(p){
 if(!p||!$("printSt"))return;
 const T=D[L],k={disabled:T.psDisabled,unknown:T.psUnknown,offline:T.psOffline,auth:T.psAuth,bad_reply:T.psBad,
  idle:T.psIdle,preparing:T.psPrep,printing:T.psPrinting,paused:T.psPaused,finished:T.psFinished,failed:T.psFailed,
  cancelled:T.psCancelled}[p.state]||p.state;
 let t=k;
 if(p.pct!=null&&(p.state=="printing"||p.state=="paused"))t+=" "+p.pct+" %";
 if(p.remain_s!=null&&p.state=="printing"){const m=Math.round(p.remain_s/60);t+=", "+T.psLeft+" "+(m>=60?Math.floor(m/60)+"h "+String(m%60).padStart(2,"0")+"m":m+" min");}
 if(p.file)t+=" – "+p.file;
 if(p.material)t+=" ("+p.material+")";
 if(p.state=="offline"&&p.next_s!=null)t+=" – "+T.psNext+" "+Math.max(0,p.next_s)+" s";
 $("printSt").textContent=t;
}
async function savePrint(){
 const b={printOn:$("printOn").checked,printType:+$("printType").value,printHost:$("printHost").value.trim(),
  printUser:$("printUser").value.trim(),printExport:$("printExport").checked};
 if($("printPass").value)b.printPass=$("printPass").value;
 if($("printKey").value)b.printKey=$("printKey").value.trim();
 try{const r=await api("/api/config",b);
  if(r.ok){msg(D[L].saved,"ok");CFG=await(await fetch("/api/config")).json();printFill();setTimeout(status,2500);}
  else msg(D[L].failed,"err");}catch(e){msg(D[L].failed,"err")}
}
async function forgetPrint(){
 try{const r=await api("/api/config",{printForget:true});
  if(r.ok){CFG=await(await fetch("/api/config")).json();printFill();msg(D[L].printForgotten,"ok");}}catch(e){msg(D[L].failed,"err")}
}

// --- Exchange rates (0.9.6) ------------------------------------------------------
// The device decides what "automatic" and "default" mean (fxHomeAuto, fxListEff),
// so after every change the page reads the config back instead of guessing.
const FX_HOMES=["CZK","EUR","PLN","HUF","CHF","GBP","DKK","SEK","NOK","RON","USD"];
const FX_CODES=["EUR","USD","GBP","CHF","PLN","HUF","CZK","JPY","CNY","DKK","SEK","NOK","RON","TRY","CAD","AUD"];
const FXN={
 cs:{EUR:"euro",USD:"americký dolar",GBP:"britská libra",CHF:"švýcarský frank",PLN:"polský zlotý",HUF:"maďarský forint",CZK:"česká koruna",JPY:"japonský jen",CNY:"čínský jüan",DKK:"dánská koruna",SEK:"švédská koruna",NOK:"norská koruna",RON:"rumunský lei",TRY:"turecká lira",CAD:"kanadský dolar",AUD:"australský dolar"},
 en:{EUR:"euro",USD:"US dollar",GBP:"pound sterling",CHF:"Swiss franc",PLN:"Polish zloty",HUF:"Hungarian forint",CZK:"Czech koruna",JPY:"Japanese yen",CNY:"Chinese yuan",DKK:"Danish krone",SEK:"Swedish krona",NOK:"Norwegian krone",RON:"Romanian leu",TRY:"Turkish lira",CAD:"Canadian dollar",AUD:"Australian dollar"},
 sk:{EUR:"euro",USD:"americký dolár",GBP:"britská libra",CHF:"švajčiarsky frank",PLN:"poľský zlotý",HUF:"maďarský forint",CZK:"česká koruna",JPY:"japonský jen",CNY:"čínsky jüan",DKK:"dánska koruna",SEK:"švédska koruna",NOK:"nórska koruna",RON:"rumunský lei",TRY:"turecká líra",CAD:"kanadský dolár",AUD:"austrálsky dolár"}};
let FXSEL=[];
function fxHomeEff(){return CFG.fxHome||CFG.fxHomeAuto||"CZK";}
function fxFill(){
 const T=D[L],N=FXN[L];
 $("fxHome").innerHTML="<option value=''>"+T.fxAuto+" ("+(CFG.fxHomeAuto||"–")+")</option>"+
  FX_HOMES.map(c=>"<option value='"+c+"'>"+c+" – "+N[c]+"</option>").join("");
 $("fxHome").value=CFG.fxHome||"";
 FXSEL=(CFG.fxListEff||[]).slice();
 fxGrid();
 crFill();
}
// --- Cryptocurrencies (0.9.7) ---
const CR_CODES=["BTC","ETH","SOL","XRP","BNB","ADA","DOGE","TRX","LTC","DOT","LINK","USDT"];
const CRN={BTC:"Bitcoin",ETH:"Ethereum",SOL:"Solana",XRP:"XRP",BNB:"BNB",ADA:"Cardano",DOGE:"Dogecoin",TRX:"TRON",LTC:"Litecoin",DOT:"Polkadot",LINK:"Chainlink",USDT:"Tether"};
let CRSEL=[];
function crFill(){
 CRSEL=(CFG.crListEff||[]).slice();
 crGrid();
 $("cgKey").value="";
 $("cgKey").placeholder=CFG.cgHasKey?"••••••••":"";
 $("cgKeySt").textContent=CFG.cgHasKey?D[L].crKeyOn:D[L].crKeyOff;
 $("cgKeyDel").disabled=!CFG.cgHasKey;
}
function crGrid(){
 $("crGrid").innerHTML=CR_CODES.map(c=>{
  const on=CRSEL.includes(c);
  const dis=(!on&&CRSEL.length>=6)||(on&&CRSEL.length==1);
  return "<label class='chk'"+(dis&&!on?" style='opacity:.45'":"")+"><input type='checkbox' data-cr='"+c+"'"+(on?" checked":"")+(dis?" disabled":"")+
   "><span><b>"+c+"</b> "+CRN[c]+"</span></label>";}).join("");
 $("crGrid").querySelectorAll("input").forEach(e=>e.onchange=()=>{
  const c=e.dataset.cr;
  CRSEL=e.checked?CRSEL.concat([c]):CRSEL.filter(x=>x!=c);
  CRSEL=CR_CODES.filter(x=>CRSEL.includes(x));
  crGrid();fxSave({crList:CRSEL});});
 $("crCount").textContent=D[L].crCount.replace("{n}",CRSEL.length);
}
function fxGrid(){
 const N=FXN[L],home=fxHomeEff();
 $("fxGrid").innerHTML=FX_CODES.map(c=>{
  const on=FXSEL.includes(c);
  // Not the home currency, not a seventh one, and not the last one off -
  // an empty list would mean "default" to the device and come back filled.
  const dis=c==home||(!on&&FXSEL.length>=6)||(on&&FXSEL.length==1);
  return "<label class='chk'"+(dis&&!on?" style='opacity:.45'":"")+"><input type='checkbox' data-fx='"+c+"'"+(on?" checked":"")+(dis?" disabled":"")+
   "><span><b>"+c+"</b> "+N[c]+"</span></label>";}).join("");
 $("fxGrid").querySelectorAll("input").forEach(e=>e.onchange=()=>{
  const c=e.dataset.fx;
  FXSEL=e.checked?FXSEL.concat([c]):FXSEL.filter(x=>x!=c);
  FXSEL=FX_CODES.filter(x=>FXSEL.includes(x));    // the order of the list above
  fxGrid();fxSave({fxList:FXSEL});});
 $("fxCount").textContent=D[L].fxCount.replace("{n}",FXSEL.length);
 $("fxSrc").textContent=home=="CZK"?D[L].fxSrcCnb:D[L].fxSrcEcb;
}
async function fxSave(o){
 try{const r=await api("/api/config",o);
  if(r.ok){CFG=await(await fetch("/api/config")).json();fxFill();msg(D[L].autoSaved,"ok");}
  else if(r.status!=403)msg(D[L].failed,"err");}catch(e){msg(D[L].failed,"err")}
}
$("fxHome").addEventListener("change",()=>fxSave({fxHome:$("fxHome").value}));

// The admin password has its own endpoint and its own button: it is never
// sent with Save, so a typo in the field that unlocks a reset cannot end up
// as the new password.
async function changePw(){
 const n=$("newPass").value,n2=$("newPass2").value;
 if(n!==n2){msg(D[L].pwMismatch,"warn");return;}
 if(!n&&!confirm(D[L].pwOffConfirm))return;
 try{const r=await api("/api/password",{password:$("pwCur").value,newPassword:n});
  if(r.ok){const j=await r.json();CFG.hasPassword=!!j.hasPassword;pwState();
   ["pwCur","newPass","newPass2"].forEach(i=>$(i).value="");
   msg(j.hasPassword?D[L].pwChanged:D[L].pwRemoved,"ok");}
  else if(r.status!=403)msg(D[L].failed,"err");
 }catch(e){msg(D[L].failed,"err")}
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
  (CFG.printOn?
   S("MPR Tisk stav","value_json.print.state | default('off', true)")+
   S("MPR Tisk prubeh","value_json.print.progress","%",null,
    "        availability: "+q+"{{ value_json.print is defined and value_json.print.progress is not none }}"+q+"\n")+
   S("MPR Tisk zbyva","value_json.print.remaining_s","s","duration",
    "        availability: "+q+"{{ value_json.print is defined and value_json.print.remaining_s is not none }}"+q+"\n")+
   S("MPR Tryska","value_json.print.nozzle_c","°C","temperature",
    "        availability: "+q+"{{ value_json.print is defined and value_json.print.nozzle_c is not none }}"+q+"\n")+
   S("MPR Podlozka","value_json.print.bed_c","°C","temperature",
    "        availability: "+q+"{{ value_json.print is defined and value_json.print.bed_c is not none }}"+q+"\n"):"")+
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
function pickCity(){const v=$("geoSel").value.split(",");$("lat").value=(+v[0]).toFixed(4);$("lon").value=(+v[1]).toFixed(4);saveLoc();}
// With the printer's secrets included the export goes through POST behind the
// admin password, and the file is put together here; otherwise a plain link.
async function exportCfg(){
 if(!CFG.printExport){location="/api/export";return;}
 try{const r=await api("/api/export",{password:$("adminPass").value});if(!r.ok){if(r.status!=403)msg(D[L].failed,"err");return;}
  const u=URL.createObjectURL(await r.blob()),a=document.createElement("a");
  a.href=u;a.download="meteoplaneradar.json";document.body.appendChild(a);a.click();a.remove();
  setTimeout(()=>URL.revokeObjectURL(u),1000);}catch(e){msg(D[L].failed,"err")}
}
async function importCfg(el){
 const f=el.files[0];if(!f)return;const t=await f.text();
 el.value="";
 let cfg;try{cfg=JSON.parse(t);}catch(e){msg(D[L].failed,"err");return;}
 const r=await api("/api/import",{password:$("adminPass").value,config:cfg});
 if(r.status!=403)msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");if(r.ok)load();
}
async function doReboot(){restartNow();}
async function doReset(){
 if(!confirm(D[L].confirmReset))return;
 const r=await api("/api/reset",{password:$("adminPass").value});
 if(r.status!=403)msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");
}
load();
</script></body></html>)rawliteral";
