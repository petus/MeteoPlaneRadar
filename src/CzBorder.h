// MeteoPlaneRadar - vyvoj / development: chiptron.cz
// =============================================================================
//  MeteoPlaneRadar - podkres map (obrys CR + mesta jako GPS body).
//  Prevzato beze zmeny z petus/Meteo-PlaneRadar.
//
//  Obrys i mesta jsou ulozene jako GPS souradnice a promitaji se STEJNOU
//  projekci jako letadla (pres callback ProjectFn), takze podkres vzdy sedi
//  s pozicemi letadel bez ohledu na polohu a zvoleny rozsah.
// =============================================================================
#pragma once
#include <Arduino.h>

// Callback: prevede lat/lon na obrazovkove souradnice (dodava ScreenPlanes).
typedef void (*ProjectFn)(float lat, float lon, int* sx, int* sy);

// Vykresli obrys CR danou barvou.
void CzBorder_Draw(ProjectFn project, uint16_t color);

// Vykresli mesta (tecka + nazev/zkratka).
// cx, cy, radius = kruh, mimo ktery se mesta nekresli.
// showFull  = true -> plne nazvy, false -> zkratky.
// showSmall = true -> i mensi mesta, false -> jen velka.
void CzBorder_DrawCities(ProjectFn project, int cx, int cy, int radius,
                         uint16_t dotColor, uint16_t textColor,
                         bool showFull, bool showSmall);
