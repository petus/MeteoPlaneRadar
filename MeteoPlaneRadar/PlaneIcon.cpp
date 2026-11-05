// =============================================================================
//  MeteoPlaneRadar
//  Aircraft icons by type - see PlaneIcon.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "PlaneIcon.h"
#include "UI.h"
#include <math.h>
#include <string.h>
#include <ctype.h>

// -----------------------------------------------------------------------------
//  Type lists
//
//  Space separated and space padded, so " A320 " cannot match inside "A3200".
//  Upper case; the lookup upper-cases the incoming code. Mostly the lists from
//  adsb-radar.com, plus the common types it does not name (Robinson helicopters,
//  Zlins, Cirrus, Diamonds, the Airbus/Boeing variants with newer designators).
// -----------------------------------------------------------------------------
static const char L_HELI[] =
  " EC20 F3 F03 EC25 EC30 EC35 EC35E EC135 EC135T2 EC145 EC145T EC145T2 EC145T2M EC155"
  " EC155B EC155B1 EC175 AS32 AS350 AS355 AS365 AS365N AS365N2 A129 A129D A139 BK117"
  " BK117A BK117B BK117C BK117D BK117C1 BK117C2 BK117C3 BK117C4 BK117C5 H60 H60M H145"
  " H145M H47 H64 H65 S76 MI8 MI17 MI24 MI28 MI35 MI26 KA27 KA32 KA50 KA52 EC45 B429"
  " A109 BK17 EC75 EC55 EXPL AS50 AS55 AS65 H160 R22 R44 R66 B06 B407 B412 B212 A169"
  " A189 S92 NH90 EH10 UH1 H500 ";
static const char L_TILT[] = " B609 AW609 V22 ";
static const char L_GLIDER[] =
  " A21C ARCP ARCUS ARCUSM ARCUST ASK13 ASK18 ASK21 ASK23 AS15 AS17 AS19 AS20 AS22 AS24"
  " AS27 AS28 AS29 AS30 ASTI TWINASTI B1 CALIFA21 DG1000 DG1000T DG300 DG40 DG400 DG505"
  " DG600 DG808 DISCUS DISCUS2 DISCUS3 DISC DUOD F01 F1 F02 F2 FOX GLID HPH304 HPH304E"
  " HPH304S JANM JANUSC JANC JANS JS1 JS3 JS5 JS7 JS8 KA6 KA6CR K6 LA12 LA17 LA19 L13 L23"
  " LIBE LS01 LS03 LS04 LS06 LS07 LS08 LS09 LS10 LS11 LS4 LS6 LS8 LS9 MINI-NIMBUS"
  " MISTRALC PERKOZ PHOE PHOEBUS PIK20 PIK20B PIK20D PIK20E SWIFT S1 SZ30 SZ36 SZ48 SZ50"
  " TAURUS ULTRALIGH VEN2 VENTUS2 VEN3 VENTUS3 VENB VENTUSB VENC VENTUSC VENT B4 F6 F7"
  " F06 F07 PARAGLIDE ULAC B4208 FOKA4 FOKA5 FOKA6 FOKA7 SZD56 DIANA2 SZD59 SZD54 SZD48"
  " SZD42 SZD41 ASW20 ASW22 ASW24 ASW27 ASW28 ASW29 ASW30 ";
static const char L_FIGHTER[] =
  " HAWK L39 L159 ALPH G4SUPER T50 YK130 M346 EUFI RALE RFAL SU57 SU27 MG29 F15 F16 F18H"
  " F18S F22 F35 F5 AJET T38 TOR GRIF MIR2 ";
static const char L_FREIGHT[] = " A124 A225 IL76 C5 C5M C17 AN22 ";
static const char L_MILTRANS[] = " C130 C130H C130J C130J30 C30J C27J C160 A400 AN12 ";
static const char L_QUAD[] =
  " A340 A342 A343 A345 A346 A380 A388 B741 B742 B743 B744 B747 B748 B74D B74S B74R BLCF"
  " BSCA C135 K35R B703 E3TF E3CF IL96 ";
static const char L_WIDE[] =
  " A300 A306 A30B A310 A330 A332 A333 A338 A339 A350 A359 A35K A3ST B762 B763 B764 B767"
  " B772 B773 B777 B778 B779 B77L B77W B787 B788 B789 B78X MD11 DC10 ";
static const char L_NARROW[] =
  " A318 A319 A320 A321 A19N A20N A21N BCS1 BCS3 C919 B727 B731 B732 B733 B734 B735 B736"
  " B737 B738 B739 B37M B38M B39M B3XM B752 B753 B757 ";
static const char L_REGIONAL[] =
  " CRJX CRJ CRJ1 CRJ2 CRJ7 CRJ9 CRJ100 CRJ200 CRJ440 CRJ700 CRJ705 CRJ900 CRJ900ER"
  " CRJ1000 CRJ1000ER E170 E175 E75S E75L E190 E195 E290 E295 ERJ ERJ135 ERJ140 ERJ145"
  " E135 E145 E35L F70 F100 MD81 MD82 MD83 MD87 MD88 MD90 MD95 B712 BA46 B461 B462 B463"
  " AT146 AVRO RJ RJ70 RJ85 RJ1H AVRORJ70 AVRORJ85 AVRORJ100 SU95 A148 J328 ";
static const char L_BIZJET[] =
  " FA7X FA8X FA10 FA20 FA50 FA900 FA2000 FA2000LX FA2000S FA2000EX F900 F2TH G650 GLF3"
  " GLF4 GLF4E GLF5 GLF6 GLF7 GL7T GL5T GLEX G450 BE40 E50P E55P E500 E550 CL60 CL65 CL35"
  " CL30 C25A C25B C25C C525 C525A C525B C525C C510 C510A C510B C56X C560 C560XL C56"
  " C680 C680A C68A C650 C650ER C750 C750ER LJ35 LJ35A LJ35A3 LJ45 LJ45EA LJ45XR LJ60"
  " LJ60XR LJ75 PHENOM100 PHENOM300 HAWK100 HAWK125 H25B H25C PRM1 HDJT SF50 PC24 EA50 ";
static const char L_TWINPROP[] =
  " DH8A DH8B DH8C DH8D AT43 AT45 AT46 AT72 AT75 AT76 DA42 DA62 BE20 B350 BE30 BE9L"
  " BE9T BE10 SW4 SF34 C408 B190 BE58 E120 PA44 L410 P06T PAY1 BE76 D28D D228 D328 JS31"
  " JS32 JS41 SB20 F50 C212 CN35 BN2P DHC6 CL2P CL2T CL4T CL215 CL415 CANSO PBY ";
static const char L_LIGHT[] =
  " B209 C150 C152 C172 C172R C172S C182 C206 C210 C340 CRUZ M20 M20J M20K M20P M20T"
  " C208 PC12 BE33 BE35 BE36 SIRA JFOX P46T FK9 F260 PC6T DR40 SLG2 SLG4 SLGP SLG5 VL3"
  " BR23 BR24 CT2K CTSW CTLS FK14 ICP2 ICP3 EV97 EVSS WT9 WT91 RANS TL20 TL22 TL200 TL300"
  " P96G TEC2 TEC4 STIN SKYR CH60 CH70 CH80 CH200 CH650 P68 P68T VTOR FDCT PA11 PA12 PA14"
  " PA15 PA16 PA17 PA18 PA20 PA22 PA24 PA25 P28A P28B P28R P28S P28T P28U PA32 PA38 TWEN"
  " SR20 SR22 DA20 DA40 C42 Z42 Z43 Z142 Z242 Z26 Z37 DHC2 DHC3 ";

struct KindList { const char* list; uint8_t kind; };
// Order matters only for the few codes on two lists; the more specific kind
// comes first.
static const KindList LISTS[] = {
  { L_HELI, PI_HELI }, { L_TILT, PI_TILTROTOR }, { L_GLIDER, PI_GLIDER },
  { L_FIGHTER, PI_FIGHTER }, { L_FREIGHT, PI_FREIGHTER }, { L_MILTRANS, PI_MILTRANS },
  { L_QUAD, PI_QUAD }, { L_WIDE, PI_WIDE }, { L_NARROW, PI_NARROW },
  { L_REGIONAL, PI_REGIONAL }, { L_BIZJET, PI_BIZJET }, { L_TWINPROP, PI_TWINPROP },
  { L_LIGHT, PI_LIGHT },
};

struct Prefix { const char* p; uint8_t kind; };
static const Prefix PREFIXES[] = {
  { "B73", PI_NARROW }, { "B75", PI_NARROW }, { "B74", PI_QUAD },
  { "B76", PI_WIDE }, { "B77", PI_WIDE }, { "B78", PI_WIDE },
  { "A31", PI_NARROW }, { "A32", PI_NARROW }, { "A2", PI_NARROW },
  { "A33", PI_WIDE }, { "A35", PI_WIDE }, { "A34", PI_QUAD }, { "A38", PI_QUAD },
  { "CRJ", PI_REGIONAL }, { "ERJ", PI_REGIONAL }, { "E17", PI_REGIONAL },
  { "E19", PI_REGIONAL }, { "E29", PI_REGIONAL },
  { "DH8", PI_TWINPROP }, { "AT4", PI_TWINPROP }, { "AT7", PI_TWINPROP },
  { "LJ", PI_BIZJET }, { "GLF", PI_BIZJET }, { "FA", PI_BIZJET }, { "C25", PI_BIZJET },
  { "C5", PI_BIZJET }, { "C6", PI_BIZJET }, { "C7", PI_BIZJET },
  { "EC", PI_HELI }, { "MI", PI_HELI }, { "KA", PI_HELI }, { "R4", PI_HELI },
  { "R6", PI_HELI }, { "H1", PI_HELI }, { "H6", PI_HELI },
  { "C1", PI_LIGHT }, { "C2", PI_LIGHT }, { "PA", PI_LIGHT }, { "P28", PI_LIGHT },
  { "SR2", PI_LIGHT }, { "DA4", PI_LIGHT }, { "DA2", PI_LIGHT }, { "Z", PI_LIGHT },
};

static bool inList(const char* list, const char* code) {
  char key[16];
  const size_t n = strlen(code);
  if (n == 0 || n > sizeof(key) - 3) return false;
  key[0] = ' ';
  memcpy(key + 1, code, n);
  key[n + 1] = ' ';
  key[n + 2] = '\0';
  return strstr(list, key) != nullptr;
}

uint8_t PlaneIcon_Classify(const char* type, const char* cat) {
  char code[12] = "";
  if (type) {
    size_t i = 0;
    for (; type[i] && i < sizeof(code) - 1; i++) code[i] = (char)toupper((unsigned char)type[i]);
    code[i] = '\0';
  }
  if (code[0]) {
    for (const KindList& k : LISTS) if (inList(k.list, code)) return k.kind;
    for (const Prefix& p : PREFIXES)
      if (strncmp(code, p.p, strlen(p.p)) == 0) return p.kind;
  }
  if (cat && cat[0] && cat[1]) {
    const char c0 = (char)toupper((unsigned char)cat[0]), c1 = cat[1];
    if (c0 == 'A') {
      switch (c1) {
        case '1': return PI_LIGHT;      // light, < 15 500 lb
        case '2': return PI_BIZJET;     // small, < 75 000 lb
        case '3': return PI_NARROW;     // large
        case '4': return PI_NARROW;     // high vortex large (B757)
        case '5': return PI_WIDE;       // heavy
        case '6': return PI_FIGHTER;    // high performance
        case '7': return PI_HELI;       // rotorcraft
      }
    } else if (c0 == 'B') {
      switch (c1) {
        case '1': return PI_GLIDER;
        case '2': return PI_BALLOON;
        case '4': return PI_LIGHT;      // ultralight / hang glider
        case '6': return PI_DRONE;
      }
    }
  }
  return PI_GENERIC;
}

const char* PlaneIcon_Name(uint8_t kind) {
  static const char* const N[PI_KIND_N] = {
    "aircraft", "light", "turboprop", "business jet", "regional jet", "narrow-body",
    "wide-body", "four-engine", "military transport", "heavy freighter", "fighter",
    "helicopter", "tiltrotor", "glider", "balloon", "drone" };
  return kind < PI_KIND_N ? N[kind] : N[0];
}

// -----------------------------------------------------------------------------
//  Shapes
//
//  Each shape is a list of CONVEX parts in local units: x to the right of the
//  nose, y forward. A part is { count, mirror, x0,y0, x1,y1, ... }; mirror = 1
//  draws it again flipped to the other side, which is how both wings come from
//  one entry. The list ends with a 0. Convex parts can be filled as a triangle
//  fan from their first vertex - no general polygon filler needed.
// -----------------------------------------------------------------------------
static const int8_t SH_GENERIC[] = {   // the original arrow, as one star-shaped fan
  10, 0,  0,12, 3,1, 13,-8, 3,-5, 3,-7, 0,-12, -3,-7, -3,-5, -13,-8, -3,1,
  0 };
static const int8_t SH_NARROW[] = {
  7, 0,  0,14, 2,11, 2,-10, 1,-14, -1,-14, -2,-10, -2,11,   // fuselage
  4, 1,  2,4, 13,-3, 13,-5, 2,-1,                          // wing
  4, 1,  4,1, 6,0, 6,-3, 4,-3,                             // engine
  4, 1,  1,-9, 6,-12, 6,-14, 1,-12,                        // tailplane
  0 };
static const int8_t SH_WIDE[] = {
  7, 0,  0,14, 3,10, 3,-10, 1,-14, -1,-14, -3,-10, -3,10,
  4, 1,  3,4, 15,-4, 15,-6, 3,-1,
  4, 1,  5,2, 8,1, 8,-3, 5,-3,
  4, 1,  1,-9, 7,-12, 7,-14, 1,-12,
  0 };
static const int8_t SH_QUAD[] = {
  7, 0,  0,14, 3,10, 3,-10, 1,-14, -1,-14, -3,-10, -3,10,
  4, 1,  3,4, 15,-4, 15,-6, 3,-1,
  4, 1,  5,2, 7,1, 7,-2, 5,-2,
  4, 1,  9,-1, 11,-2, 11,-5, 9,-5,
  4, 1,  1,-9, 7,-12, 7,-14, 1,-12,
  0 };
static const int8_t SH_REGIONAL[] = {
  7, 0,  0,13, 2,10, 2,-9, 1,-13, -1,-13, -2,-9, -2,10,
  4, 1,  2,3, 11,-2, 11,-4, 2,-1,
  4, 1,  2,-5, 4,-5, 4,-9, 2,-9,                           // rear engines
  4, 1,  0,-11, 6,-12, 6,-13, 0,-13,                       // T-tail
  0 };
static const int8_t SH_BIZJET[] = {
  7, 0,  0,12, 2,8, 2,-8, 1,-12, -1,-12, -2,-8, -2,8,
  4, 1,  2,2, 10,-4, 10,-5, 2,-1,
  4, 1,  2,-5, 4,-5, 4,-8, 2,-8,
  4, 1,  0,-10, 5,-12, 5,-13, 0,-12,
  0 };
static const int8_t SH_TWINPROP[] = {
  6, 0,  0,13, 2,10, 2,-11, 0,-13, -2,-11, -2,10,
  4, 1,  2,3, 14,2, 14,-1, 2,-1,                           // straight wing
  4, 1,  4,6, 6,6, 6,-1, 4,-1,                             // nacelle ahead of the wing
  4, 1,  1,-9, 6,-10, 6,-12, 1,-12,
  0 };
static const int8_t SH_LIGHT[] = {
  6, 0,  0,10, 2,8, 2,-9, 0,-11, -2,-9, -2,8,
  4, 1,  2,5, 14,5, 14,2, 2,2,                             // long straight high wing
  4, 1,  1,-8, 5,-8, 5,-10, 1,-10,
  0 };
static const int8_t SH_MILTRANS[] = {
  6, 0,  0,13, 3,10, 3,-10, 0,-13, -3,-10, -3,10,
  4, 1,  3,4, 16,3, 16,0, 3,0,
  4, 1,  5,7, 7,7, 7,0, 5,0,
  4, 1,  10,6, 12,6, 12,0, 10,0,
  4, 1,  1,-9, 7,-10, 7,-12, 1,-12,
  0 };
static const int8_t SH_FREIGHTER[] = {
  7, 0,  0,14, 3,10, 3,-10, 1,-14, -1,-14, -3,-10, -3,10,
  4, 1,  3,5, 16,-1, 16,-3, 3,1,                           // shoulder wing, mild sweep
  4, 1,  6,3, 8,2, 8,-1, 6,-1,
  4, 1,  11,1, 13,0, 13,-3, 11,-3,
  4, 1,  0,-11, 7,-13, 7,-14, 0,-14,
  0 };
static const int8_t SH_FIGHTER[] = {
  6, 0,  0,13, 2,8, 2,-11, 1,-13, -1,-13, -2,-11,
  3, 1,  1,6, 10,-6, 1,-6,                                 // delta
  3, 1,  1,-8, 5,-12, 1,-12,
  0 };
static const int8_t SH_GLIDER[] = {
  6, 0,  0,10, 1,8, 1,-10, 0,-11, -1,-10, -1,8,
  4, 1,  1,3, 17,2, 17,1, 1,1,                             // very long, very thin
  4, 1,  0,-9, 4,-9, 4,-10, 0,-10,
  0 };
static const int8_t SH_TILT[] = {
  6, 0,  0,12, 2,9, 2,-10, 0,-12, -2,-10, -2,9,
  4, 1,  2,2, 10,2, 10,-1, 2,-1,
  4, 1,  1,-8, 5,-9, 5,-11, 1,-11,
  0 };

static const int8_t* shapeOf(uint8_t k) {
  switch (k) {
    case PI_NARROW:    return SH_NARROW;
    case PI_WIDE:      return SH_WIDE;
    case PI_QUAD:      return SH_QUAD;
    case PI_REGIONAL:  return SH_REGIONAL;
    case PI_BIZJET:    return SH_BIZJET;
    case PI_TWINPROP:  return SH_TWINPROP;
    case PI_LIGHT:     return SH_LIGHT;
    case PI_MILTRANS:  return SH_MILTRANS;
    case PI_FREIGHTER: return SH_FREIGHTER;
    case PI_FIGHTER:   return SH_FIGHTER;
    case PI_GLIDER:    return SH_GLIDER;
    case PI_TILTROTOR: return SH_TILT;
    default:           return SH_GENERIC;
  }
}

// Size says weight at a glance - the point the adsb-radar.com scheme makes.
static float scaleOf(uint8_t k) {
  switch (k) {
    case PI_LIGHT:     return 0.80f;
    case PI_GLIDER:    return 0.85f;
    case PI_BIZJET:    return 0.85f;
    case PI_FIGHTER:   return 0.90f;
    case PI_TWINPROP:  return 0.90f;
    case PI_REGIONAL:  return 0.95f;
    case PI_WIDE:      return 1.25f;
    case PI_MILTRANS:  return 1.15f;
    case PI_QUAD:      return 1.40f;
    case PI_FREIGHTER: return 1.40f;
    default:           return 1.00f;
  }
}

void PlaneIcon_Draw(int x, int y, float trackDeg, bool hasTrack, uint8_t kind, uint16_t col) {
  // Balloons and drones have no meaningful nose; draw them upright.
  if (kind == PI_BALLOON) {
    gfx->fillCircle(x, y - 3, 7, col);
    gfx->drawLine(x - 3, y + 3, x - 2, y + 7, col);
    gfx->drawLine(x + 3, y + 3, x + 2, y + 7, col);
    gfx->fillRect(x - 2, y + 7, 5, 3, col);
    return;
  }
  if (kind == PI_DRONE) {
    gfx->drawLine(x - 7, y - 7, x + 7, y + 7, col);
    gfx->drawLine(x - 7, y + 7, x + 7, y - 7, col);
    gfx->drawCircle(x - 7, y - 7, 3, col); gfx->drawCircle(x + 7, y - 7, 3, col);
    gfx->drawCircle(x - 7, y + 7, 3, col); gfx->drawCircle(x + 7, y + 7, 3, col);
    gfx->fillRect(x - 2, y - 2, 5, 5, col);
    return;
  }
  if (!hasTrack) {
    // Track unknown - circle with a dot (orientation cannot be determined).
    gfx->drawCircle(x, y, 7, col);
    gfx->fillCircle(x, y, 2, col);
    return;
  }

  const float a = trackDeg * 0.0174532925f;
  const float ca = cosf(a), sa = sinf(a);

  if (kind == PI_HELI) {
    // Rotor disc, cabin, tail boom with a small tail rotor. The disc is what
    // makes it read as a helicopter even at the size of a thumbnail.
    gfx->drawCircle(x, y, 11, col);
    gfx->fillCircle(x, y, 4, col);
    const int tx = x - (int)(13 * sa), ty = y + (int)(13 * ca);
    gfx->drawLine(x, y, tx, ty, col);
    gfx->drawLine(x + 1, y, tx + 1, ty, col);                 // two pixels wide
    gfx->drawLine(tx - (int)(3 * ca), ty - (int)(3 * sa), tx + (int)(3 * ca), ty + (int)(3 * sa), col);
    return;
  }

  const float s = scaleOf(kind);
  // Same mapping as the original arrow: right = to the right of the nose,
  // fwd = towards it; track 0 points up.
  auto rot = [&](float right, float fwd, int* ox, int* oy) {
    right *= s; fwd *= s;
    *ox = x + (int)lroundf(right * ca + fwd * sa);
    *oy = y + (int)lroundf(right * sa - fwd * ca);
  };

  const int8_t* p = shapeOf(kind);
  while (*p) {
    const int n = p[0];
    const bool mirror = p[1] != 0;
    const int8_t* v = p + 2;
    for (int side = 0; side < (mirror ? 2 : 1); side++) {
      const float m = side ? -1.0f : 1.0f;
      int px[10], py[10];
      for (int i = 0; i < n && i < 10; i++) rot(m * v[2 * i], v[2 * i + 1], &px[i], &py[i]);
      if (p == SH_GENERIC) {
        // The arrow is not convex - fan it from the centre as it always was.
        for (int i = 0; i < n; i++) {
          const int j = (i + 1) % n;
          gfx->fillTriangle(x, y, px[i], py[i], px[j], py[j], col);
        }
      } else {
        for (int i = 1; i + 1 < n; i++)
          gfx->fillTriangle(px[0], py[0], px[i], py[i], px[i + 1], py[i + 1], col);
      }
    }
    p += 2 + 2 * n;
  }
}
