// Host stand-in for Arduino.h - just enough for the cards screen to compile on a PC.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <string>

#define PROGMEM
// Print.h on the device defines these as plain macros; having them here too means a
// name that collides with one fails on the PC as it would on the device.
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

unsigned long millis();   // fake clock owned by the harness

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) { size_t k = 0; while (n--) k += write(*buf++); return k; }
  size_t write(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t print(const char* s) { return write((const uint8_t*)s, strlen(s)); }
  size_t println(const char* s = "") { size_t n = print(s); return n + write((uint8_t)'\n'); }
  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char b[512]; va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    return print(b);
  }
};
class Printable { public: virtual ~Printable() {} virtual size_t printTo(Print& p) const = 0; };
class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual size_t readBytes(char* buf, size_t n) {
    size_t k = 0; while (k < n) { int c = read(); if (c < 0) break; buf[k++] = (char)c; } return k;
  }
};

class String {
  std::string s_;
 public:
  String() {}
  String(const char* c) : s_(c ? c : "") {}
  String& operator=(const char* c) { s_ = c ? c : ""; return *this; }
  const char* c_str() const { return s_.c_str(); }
  size_t length() const { return s_.size(); }
  bool concat(const char* c) { if (c) s_ += c; return true; }
  String& operator+=(const char* c) { concat(c); return *this; }
  String& operator+=(const String& o) { s_ += o.s_; return *this; }
  bool endsWith(const char* suf) const {
    const size_t n = strlen(suf);
    return s_.size() >= n && s_.compare(s_.size() - n, n, suf) == 0;
  }
  void assign(const std::string& v) { s_ = v; }
};

struct SerialShim : Print {
  using Print::write;
  size_t write(uint8_t c) override { fputc(c, stdout); return 1; }
};
extern SerialShim Serial;
