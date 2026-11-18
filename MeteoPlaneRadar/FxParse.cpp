// =============================================================================
//  MeteoPlaneRadar
//  Exchange rates - parsing (0.9.6). See FxParse.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "FxParse.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// A number with either decimal separator: "24,350" or "1.0812".
static bool numberOf(const char* s, size_t n, double* v) {
  char b[32];
  size_t k = 0;
  while (n && (*s == ' ' || *s == '"')) { s++; n--; }
  while (n && (s[n - 1] == ' ' || s[n - 1] == '"' || s[n - 1] == '\r')) n--;
  if (n == 0 || n >= sizeof(b)) return false;
  for (size_t i = 0; i < n; i++) {
    char c = s[i];
    if (c == ',') c = '.';
    if (!((c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    b[k++] = c;
  }
  b[k] = '\0';
  char* end = nullptr;
  *v = strtod(b, &end);
  return end && *end == '\0';
}

static bool codeOf(const char* s, size_t n, char out[4]) {
  while (n && (*s == ' ' || *s == '"')) { s++; n--; }
  while (n && (s[n - 1] == ' ' || s[n - 1] == '"' || s[n - 1] == '\r')) n--;
  if (n != 3) return false;
  for (int i = 0; i < 3; i++) if (s[i] < 'A' || s[i] > 'Z') return false;
  memcpy(out, s, 3);
  out[3] = '\0';
  return true;
}

// Splits one line at sep into at most max fields (pointer + length).
static int split(const char* line, size_t len, char sep,
                 const char** f, size_t* fl, int max) {
  int n = 0;
  const char* p = line;
  const char* end = line + len;
  while (n < max) {
    const char* q = (const char*)memchr(p, sep, (size_t)(end - p));
    if (!q) q = end;
    f[n] = p; fl[n] = (size_t)(q - p); n++;
    if (q == end) break;
    p = q + 1;
  }
  return n;
}

int FxParse_Cnb(const char* body, long* date, FxCnbQuote* out, int max) {
  if (date) *date = 0;
  if (!body) return -1;
  int n = 0;
  bool first = true, sawHeader = false;
  for (const char* p = body; *p; ) {
    const char* e = strchr(p, '\n');
    const size_t len = e ? (size_t)(e - p) : strlen(p);
    if (first) {
      int d = 0, m = 0, y = 0;
      if (sscanf(p, "%d.%d.%d", &d, &m, &y) == 3 && d >= 1 && d <= 31 && m >= 1 && m <= 12 && y > 1990)
        if (date) *date = (long)y * 10000 + m * 100 + d;
      first = false;
    } else {
      const char* f[6]; size_t fl[6];
      if (split(p, len, '|', f, fl, 6) == 5) {
        sawHeader = true;                       // a line in the CNB shape
        double amt, rate;
        FxCnbQuote q;
        if (n < max && codeOf(f[3], fl[3], q.code) && numberOf(f[2], fl[2], &amt) &&
            numberOf(f[4], fl[4], &rate) && amt >= 1 && amt <= 10000 && rate > 0) {
          q.amount = (uint16_t)amt;
          q.rate = rate;
          out[n++] = q;
        }
      }
    }
    if (!e) break;
    p = e + 1;
  }
  return sawHeader ? n : -1;
}

static long ymdOf(const char* s, size_t n) {
  while (n && (*s == ' ' || *s == '"')) { s++; n--; }
  int y = 0, m = 0, d = 0;
  char b[16];
  if (n >= sizeof(b)) return 0;
  memcpy(b, s, n); b[n] = '\0';
  if (sscanf(b, "%d-%d-%d", &y, &m, &d) != 3) return 0;
  if (y < 1990 || m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return (long)y * 10000 + m * 100 + d;
}

int FxParse_Ecb(const char* body, FxEcbSeries* out, int max) {
  if (!body) return -1;
  int cCur = -1, cTime = -1, cVal = -1;
  int n = 0;
  bool header = true;
  for (const char* p = body; *p; ) {
    const char* e = strchr(p, '\n');
    const size_t len = e ? (size_t)(e - p) : strlen(p);
    const char* f[24]; size_t fl[24];
    const int nf = split(p, len, ',', f, fl, 24);
    if (header) {
      for (int i = 0; i < nf; i++) {
        size_t l = fl[i]; const char* s = f[i];
        while (l && (*s == '"' || *s == ' ' || (unsigned char)*s == 0xEF ||
                     (unsigned char)*s == 0xBB || (unsigned char)*s == 0xBF)) { s++; l--; }   // BOM
        while (l && (s[l - 1] == '"' || s[l - 1] == '\r' || s[l - 1] == ' ')) l--;
        if (l == 8 && !strncmp(s, "CURRENCY", 8)) cCur = i;
        else if (l == 11 && !strncmp(s, "TIME_PERIOD", 11)) cTime = i;
        else if (l == 9 && !strncmp(s, "OBS_VALUE", 9)) cVal = i;
      }
      if (cCur < 0 || cTime < 0 || cVal < 0) return -1;
      header = false;
    } else if (nf > cCur && nf > cTime && nf > cVal) {
      char code[4];
      double v;
      const long d = ymdOf(f[cTime], fl[cTime]);
      if (codeOf(f[cCur], fl[cCur], code) && d && numberOf(f[cVal], fl[cVal], &v) && v > 0) {
        int k = 0;
        while (k < n && strcmp(out[k].code, code)) k++;
        if (k == n) {
          if (n >= max) goto next;
          memcpy(out[k].code, code, 4);
          out[k].date[0] = out[k].date[1] = 0;
          out[k].val[0] = out[k].val[1] = 0;
          n++;
        }
        FxEcbSeries& s = out[k];
        // Keep the newest two, in whatever order the rows arrive.
        if (d > s.date[0]) {
          s.date[1] = s.date[0]; s.val[1] = s.val[0];
          s.date[0] = d;         s.val[0] = v;
        } else if (d != s.date[0] && d > s.date[1]) {
          s.date[1] = d; s.val[1] = v;
        }
      }
    }
  next:
    if (!e) break;
    p = e + 1;
  }
  return header ? -1 : n;
}

uint16_t FxParse_Amount(double perUnit) {
  if (!(perUnit > 0)) return 1;
  if (perUnit < 0.001) return 1000;
  if (perUnit < 0.1) return 100;
  return 1;
}

void FxParse_Format(double v, bool comma, char* out, size_t cap, int maxDec) {
  if (!(v == v) || v < 0) { snprintf(out, cap, "--"); return; }
  int dec;
  if (v >= 10000)     dec = 0;
  else if (v >= 1000) dec = 1;
  else if (v >= 100)  dec = 2;
  else if (v >= 10)   dec = 3;
  else                dec = 4;
  if (dec > maxDec) dec = maxDec;
  snprintf(out, cap, "%.*f", dec, v);
  // 9.99995 rounds up to "10.0000" - one character too many.
  while (dec > 0 && strlen(out) > 6) snprintf(out, cap, "%.*f", --dec, v);
  if (comma) for (char* c = out; *c; c++) if (*c == '.') *c = ',';
}

void FxParse_FormatPrice(double v, bool comma, bool english,
                         char* out, size_t cap, char* suf, size_t sufCap) {
  if (sufCap) suf[0] = '\0';
  if (!(v == v) || v < 0) { snprintf(out, cap, "--"); return; }
  if (v >= 999999.5) {
    const double m = v / 1e6;
    const int dec = m < 9.995 ? 2 : (m < 99.95 ? 1 : 0);
    snprintf(out, cap, "%.*f", dec, m);
    if (comma) for (char* c = out; *c; c++) if (*c == '.') *c = ',';
    snprintf(suf, sufCap, "%s", english ? "M" : "mil.");
    return;
  }
  if (v >= 9999.5) {
    const long n = (long)(v + 0.5);
    snprintf(out, cap, "%ld %03ld", n / 1000, n % 1000);
    return;
  }
  FxParse_Format(v, comma, out, cap);
}

long FxParse_DayBefore(long ymd) {
  int y = (int)(ymd / 10000), m = (int)(ymd / 100 % 100), d = (int)(ymd % 100);
  if (--d >= 1) return (long)y * 10000 + m * 100 + d;
  if (--m < 1) { m = 12; y--; }
  static const int DIM[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  d = DIM[m - 1];
  if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) d = 29;
  return (long)y * 10000 + m * 100 + d;
}
