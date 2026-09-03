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
    </div>
    <p class="hint" data-i18n="scrHint">Vypnuté obrazovky se přeskakují. Nastavení je dostupné vždy.</p>
    <p class="hint" data-i18n="restartHint">Změna obrazovek, zdroje radaru nebo polohy potřebuje restart, takže se ukládá až tlačítkem dole.</p>
    <div class="row"><label data-i18n="autoRotate">Automatické střídání (sekundy, 0 = vypnuto)</label>
      <input type="number" id="autoRotate" min="0" max="3600" step="5"></div>
    <p class="hint" data-i18n="rotHint">Střídání pozastaví přejetí prstem, dlouhý stisk nebo přepnutí z prohlížeče — na trojnásobek intervalu, pak pokračuje samo. Obyčejné klepnutí ho nezastaví, otevřený detail letadla ho drží. Na obrazovce Nastavení se nestřídá.</p>
  </div>

  <div class="card">
    <h2 data-i18n="radar">Meteoradar</h2>
    <div class="row"><label data-i18n="radarSrc">Zdroj dat</label>
      <select id="radarSrc">
        <option value="0" data-i18n="srcChmu">ČHMÚ (ostřejší, jen ČR)</option>
        <option value="1" data-i18n="srcRv">RainViewer (Evropa i svět)</option>
      </select></div>
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
  </div>
</section>

</div>

<div class="bar hide" id="saveBar"><button onclick="save()" data-i18n="save">Uložit nastavení</button><span id="msg"></span></div>

<script>
const D={
 cs:{tabCtl:"Ovládání",tabLoc:"Poloha",tabScr:"Obrazovky",tabLook:"Vzhled",tabPlanes:"Letadla",tabSys:"Systém",
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
  system:"Systém",adminPass:"Současné heslo",newPass:"Nové heslo",
  passHint:"Současné heslo je potřeba pro aktualizaci, import, reset i pro změnu hesla. Prázdné nové heslo nic nemění; jedna mezera ochranu zruší.",
  fwUpdate:"Aktualizace firmwaru",export:"Export nastavení",import:"Import",reboot:"Restart",factory:"Tovární reset",save:"Uložit nastavení",
  settings:"Nastavení",
  saved:"Uloženo",failed:"Nepovedlo se",searching:"Hledám…",nothing:"Nic nenalezeno",
  disabled:"Obrazovka je vypnutá",confirmReset:"Opravdu smazat všechna nastavení včetně WiFi?"},
 en:{tabCtl:"Control",tabLoc:"Location",tabScr:"Screens",tabLook:"Appearance",tabPlanes:"Aircraft",tabSys:"System",
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
  system:"System",adminPass:"Current password",newPass:"New password",
  passHint:"The current password is needed for updates, import, reset and to change the password itself. An empty new password changes nothing; a single space removes the protection.",
  fwUpdate:"Firmware update",export:"Export settings",import:"Import",reboot:"Reboot",factory:"Factory reset",save:"Save settings",
  settings:"Settings",
  saved:"Saved",failed:"Failed",searching:"Searching…",nothing:"Nothing found",
  disabled:"That screen is switched off",confirmReset:"Really erase all settings including WiFi?"}};
let L="cs", CFG={}, TAB="tCtl";
const $=id=>document.getElementById(id);

function setLang(v){L=(v==1||v=="en")?"en":"cs";$("uiLang").value=(L=="en")?"1":"0";
 document.documentElement.lang=L;
 document.querySelectorAll("[data-i18n]").forEach(e=>{const t=D[L][e.dataset.i18n];if(t)e.textContent=t;});
 pwState();status();}

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

const SCR=[["scrClock",0],["scrPlanes",1],["scrMeteo",2],["scrForecast",3],["settings",4]];
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
 ["watch","change","watch",e=>e.value],
 ["autoRotate","change","autoRotate",e=>+e.value],
];
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
 $("autoRotate").value=CFG.autoRotate;$("radarSrc").value=CFG.radarSrc;
 $("meteoLegend").checked=CFG.meteoLegend;
 $("briDay").value=CFG.briDay;$("briNight").value=CFG.briNight;
 $("nightAuto").checked=CFG.nightAuto;$("nightOffset").value=CFG.nightOffset;
 $("secStyle").value=CFG.secStyle;$("metric").checked=CFG.metric;$("topBearing").value=CFG.topBearing;
 $("clockColor").value=rgb565ToHex(CFG.clockColor);$("secColor").value=rgb565ToHex(CFG.secColor);
 $("altMin").value=CFG.altMin;$("altMax").value=CFG.altMax;
 pwState();
 $("onlyCallsign").checked=CFG.onlyCallsign;$("squawkAlert").checked=CFG.squawkAlert;$("watch").value=CFG.watch||"";
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
  ["Firmware","v"+s.version],[D[L].board,s.board]];
 $("statusTab").innerHTML=rows.map(r=>"<tr><td>"+r[0]+"</td><td>"+r[1]+"</td></tr>").join("");}catch(e){}
}
setInterval(status,10000);

function body(){return{lat:parseFloat($("lat").value),lon:parseFloat($("lon").value),
 lang:parseInt($("uiLang").value),metric:$("metric").checked,
 briDay:+$("briDay").value,briNight:+$("briNight").value,nightAuto:$("nightAuto").checked,
 nightOffset:+$("nightOffset").value,radarSrc:+$("radarSrc").value,autoRotate:+$("autoRotate").value,
 meteoLegend:$("meteoLegend").checked,
 topBearing:+$("topBearing").value,secStyle:+$("secStyle").value,
 clockColor:hexToRgb565($("clockColor").value),secColor:hexToRgb565($("secColor").value),
 altMin:+$("altMin").value,altMax:+$("altMax").value,onlyCallsign:$("onlyCallsign").checked,
 squawkAlert:$("squawkAlert").checked,watch:$("watch").value,
 password:$("adminPass").value,newPassword:$("newPass").value,
 screens:{clock:$("sClock").checked,planes:$("sPlanes").checked,meteo:$("sMeteo").checked,forecast:$("sForecast").checked}};}

async function save(){
 const r=await fetch("/api/config",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(body())});
 if(r.ok){msg(D[L].saved,"ok");$("newPass").value="";setLang($("uiLang").value);}else msg(D[L].failed+" ("+r.status+")","err");
}
async function scan(){
 const s=$("ssid");s.innerHTML="<option>…</option>";
 try{const l=await(await fetch("/api/scan")).json();
 s.innerHTML=l.map(n=>"<option value='"+n.ssid+"'>"+n.ssid+" ("+n.rssi+")</option>").join("");}catch(e){s.innerHTML=""}
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
async function doReboot(){await fetch("/api/reboot",{method:"POST"});msg("…");}
async function doReset(){
 if(!confirm(D[L].confirmReset))return;
 const r=await fetch("/api/reset",{method:"POST",headers:{"Content-Type":"application/json"},
  body:JSON.stringify({password:$("adminPass").value})});
 msg(r.ok?D[L].saved:D[L].failed,r.ok?"ok":"err");
}
load();
</script></body></html>)rawliteral";
