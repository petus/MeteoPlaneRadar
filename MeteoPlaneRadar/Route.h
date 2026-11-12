// =============================================================================
//  MeteoPlaneRadar
//  Kam vybrane letadlo leti - trasa z adsb.lol.
//
//  adsb.fi vozi polohy, ale ne trasu, takze ta musi prijit odjinud. Drive se
//  pouzivala ciste staticka databaze planovanych tras: jeden radek na callsign,
//  bez data a bez vazby na konkretni let. Callsigny se ale mezi rotacemi a
//  sezonami recykluji, takze letadlo nad Prahou dostalo klidne trasu
//  Atheny -> Istanbul a nebylo jak poznat, ze je spatne.
//
//  adsb.lol umi navic jednu vec: spolu s callsignem se posila i poloha letadla
//  a server vrati priznak "plausible". Pocita kolmou vzdalenost polohy od
//  ortodromy mezi letisti trasy s toleranci max(50 NM, 20 % delky trasy).
//  Trasa, ktera k poloze nesedi, se tim odfiltruje jeste na serveru.
//
//  Endpoint (bez klice, bez registrace):
//    GET https://api.adsb.lol/api/0/route/{callsign}/{lat}/{lon}
//
//  Pta se ZASE jen pri otevreni detailu jednoho letadla - jeden dotaz na jedno
//  letadlo, nikdy pro cely seznam - a odpoved se kesuje, takze prepinani mezi
//  dvema letadly uz API nezatezuje. Spousta letu trasu nema (general aviation,
//  vojenske stroje, vrtulniky) a spousta letadel nevysila callsign vubec; oboji
//  je normalni stav, ne chyba, a proste se nic nezobrazi.
//
//  Registrace a typ letadla uz sem nepatri - oboji vozi adsb.fi ve stejne
//  odpovedi, kterou stahujeme kvuli polohe (pole "r" a "t"), viz ADSB.h.
//
//  Project: MeteoPlaneRadar - live aircraft radar on a round touchscreen
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
//  Web:     https://chiptron.cz
// =============================================================================
#pragma once
#include <Arduino.h>

enum RouteState : uint8_t {
  ROUTE_IDLE = 0,   // nic se nezada
  ROUTE_WAIT,       // ve fronte / stahuje se
  ROUTE_OK,         // trasa nalezena a server ji oznacil za verohodnou
  ROUTE_NONE        // dotaz probehl, ale pouzitelna trasa neni
};

struct RouteInfo {
  char from[20] = "";   // "Prague", pripadne "PRG" kdyz mesto chybi
  char to[20]   = "";
};

// Zeptej se na tuto trasu. Levne a idempotentni: opakovane volani se stejnym
// callsignem nedela nic, jakmile je odpoved v kesi. Prazdny callsign (letadlo
// zadny nevysila) dotaz vubec nespusti - hex se sem uz neposila, protoze
// normalizovany hex je platny IATA tvar: "a31234" -> "A31234" je let Aegean
// Airlines 1234, odtud ta recka trasa u letadla nad Prahou.
// Poloha se posila spolu s callsignem, server podle ni pocita "plausible".
void       Route_Select(const char* callsign, float lat, float lon);

// Nic neni vybrano - zrus cekajici dotaz.
void       Route_Clear();

// Provede cekajici dotaz. Vola se z loop(); bez dotazu nedela nic.
void       Route_Tick();

// Yield + reset watchdogu, vola se pri kazdem bloku stahovane odpovedi.
// Bez toho projde cteni trasy dvema blokujicimi ctenimi po 8 s, tedy pres
// WDT_TIMEOUT_S, a zarizeni se restartuje uprostred stahovani.
void       Route_SetPollFn(void (*fn)());

RouteState Route_GetState();
const RouteInfo* Route_Get();   // platne, dokud je stav ROUTE_OK

// Vrati true prave jednou po tom, co Route_Tick() dopsal vysledek, a priznak
// tim zhasne. Obrazovka se prekresluje jen kdyz ma co ukazat noveho, a bez
// tohohle by na odpoved cekala az na dalsi stahovani letadel - tedy podle
// dosahu 5 az 15 sekund, i kdyz trasa dorazila hned.
bool       Route_TakeChanged();

// Folds UTF-8 text down to what the built-in 7-bit font can draw: accents are
// dropped, the letter under them stays ("Kč" -> "Kc"). cap includes the NUL.
void Route_ToAscii(char* dst, size_t cap, const char* src);
