// =============================================================================
//  MeteoPlaneRadar
//  Moon (0.9.7). See Moon.h. Ported from SunCalc (c) 2011-2015 Vladimir
//  Agafonkin, BSD-2-Clause, https://github.com/mourner/suncalc - formulas from
//  "Astronomical Algorithms" (Meeus) and http://aa.quae.nl/en/reken/hemelpositie.html
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "Moon.h"
#include <math.h>

static const double PI_  = 3.14159265358979323846;
static const double RAD  = PI_ / 180.0;
static const double E    = RAD * 23.4397;           // obliquity of the Earth

// Days since J2000.0 for a Unix time.
static double toDays(double ut) { return ut / 86400.0 - 0.5 + 2440588.0 - 2451545.0; }

static double rightAscension(double l, double b) {
  return atan2(sin(l) * cos(E) - tan(b) * sin(E), cos(l));
}
static double declination(double l, double b) {
  return asin(sin(b) * cos(E) + cos(b) * sin(E) * sin(l));
}
static double altitude(double H, double phi, double dec) {
  return asin(sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(H));
}
// Greenwich mean sidereal time (IAU 1982, enough here) minus the longitude.
// SunCalc's 280.16 is 0.3 deg off the standard 280.46 - over a minute at the
// horizon, found by the comparison with an ephemeris.
static double siderealTime(double d, double lw) { return RAD * (280.46061837 + 360.98564736629 * d) - lw; }

struct Coords { double ra, dec, dist; };

// The Moon's geocentric position with the main periodic terms (Meeus, ch. 47,
// the largest dozen): about a tenth of a degree, a minute or two at the horizon.
// SunCalc keeps only the first term of each, which put moonrise up to an hour
// out against an ephemeris.
static Coords moonCoords(double d) {
  const double Lp = RAD * (218.316 + 13.176396 * d);  // mean longitude
  const double M  = RAD * (134.963 + 13.064993 * d);  // mean anomaly
  const double F  = RAD * (93.272 + 13.229350 * d);   // argument of latitude
  const double D  = RAD * (297.850 + 12.190749 * d);  // mean elongation
  const double Ms = RAD * (357.529 + 0.98560028 * d); // the Sun's mean anomaly
  const double l = Lp + RAD * (6.289 * sin(M) + 1.274 * sin(2 * D - M) + 0.658 * sin(2 * D)
                             + 0.214 * sin(2 * M) - 0.186 * sin(Ms) - 0.114 * sin(2 * F)
                             + 0.059 * sin(2 * D - 2 * M) + 0.057 * sin(2 * D - Ms - M)
                             + 0.053 * sin(2 * D + M) + 0.046 * sin(2 * D - Ms)
                             - 0.041 * sin(M - Ms) - 0.035 * sin(D) - 0.031 * sin(M + Ms));
  const double b = RAD * (5.128 * sin(F) + 0.281 * sin(M + F) + 0.278 * sin(M - F)
                        + 0.173 * sin(2 * D - F) + 0.055 * sin(2 * D - M + F)
                        - 0.046 * sin(2 * D - M - F) + 0.033 * sin(2 * D + F)
                        + 0.017 * sin(2 * M + F));
  Coords c;
  c.ra = rightAscension(l, b);
  c.dec = declination(l, b);
  c.dist = 385001 - 20905 * cos(M) - 3699 * cos(2 * D - M) - 2956 * cos(2 * D)
         - 570 * cos(2 * M);                          // km
  return c;
}

static Coords sunCoords(double d) {
  const double M = RAD * (357.5291 + 0.98560028 * d);
  const double C = RAD * (1.9148 * sin(M) + 0.02 * sin(2 * M) + 0.0003 * sin(3 * M));
  const double L = M + C + RAD * 102.9372 + PI_;
  Coords c;
  c.ra = rightAscension(L, 0);
  c.dec = declination(L, 0);
  c.dist = 149598000;
  return c;
}

// How far the Moon's centre is above the altitude at which its upper limb
// touches the horizon (radians). Geocentric altitude against Meeus's h0 =
// 0.7275 * parallax - 0.5667 deg: refraction, the Moon's radius and its large
// parallax (~1 deg - the Moon is close) in one number.
static double moonAboveRise(double ut, double lat, double lon) {
  const double lw = RAD * -lon, phi = RAD * lat, d = toDays(ut);
  const Coords c = moonCoords(d);
  const double H = siderealTime(d, lw) - c.ra;
  const double h = altitude(H, phi, c.dec);
  const double par = asin(6378.14 / c.dist);
  return h - (0.7275 * par - 0.5667 * RAD);
}

void Moon_Times(time_t dayStart, double lat, double lon, time_t* rise, time_t* set) {
  const double t0 = (double)dayStart;
  double h0 = moonAboveRise(t0, lat, lon);
  double r = -1, s = -1;
  // Through the day two hours at a time: a parabola through three altitudes,
  // and where it crosses zero.
  for (int i = 1; i <= 24; i += 2) {
    const double h1 = moonAboveRise(t0 + i * 3600.0, lat, lon);
    const double h2 = moonAboveRise(t0 + (i + 1) * 3600.0, lat, lon);
    const double a = (h0 + h2) / 2 - h1;
    const double b = (h2 - h0) / 2;
    const double xe = -b / (2 * a);
    const double ye = (a * xe + b) * xe + h1;
    const double disc = b * b - 4 * a * h1;
    int roots = 0;
    double x1 = 0, x2 = 0;
    if (disc >= 0) {
      const double dx = sqrt(disc) / (fabs(a) * 2);
      x1 = xe - dx;
      x2 = xe + dx;
      if (fabs(x1) <= 1) roots++;
      if (fabs(x2) <= 1) roots++;
      if (x1 < -1) x1 = x2;
    }
    if (roots == 1) {
      if (h0 < 0) r = i + x1;
      else        s = i + x1;
    } else if (roots == 2) {
      r = i + (ye < 0 ? x2 : x1);
      s = i + (ye < 0 ? x1 : x2);
    }
    if (r >= 0 && s >= 0) break;
    h0 = h2;
  }
  if (rise) *rise = r >= 0 ? (time_t)(t0 + r * 3600.0 + 0.5) : 0;
  if (set)  *set  = s >= 0 ? (time_t)(t0 + s * 3600.0 + 0.5) : 0;
}

double Moon_Phase(time_t t) {
  const double d = toDays((double)t);
  const Coords s = sunCoords(d), m = moonCoords(d);
  const double phi = acos(sin(s.dec) * sin(m.dec) + cos(s.dec) * cos(m.dec) * cos(s.ra - m.ra));
  const double inc = atan2(s.dist * sin(phi), m.dist - s.dist * cos(phi));
  const double angle = atan2(cos(s.dec) * sin(s.ra - m.ra),
                             sin(s.dec) * cos(m.dec) - cos(s.dec) * sin(m.dec) * cos(s.ra - m.ra));
  return 0.5 + 0.5 * inc * (angle < 0 ? -1 : 1) / PI_;
}
