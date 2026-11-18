// =============================================================================
//  MeteoPlaneRadar
//  See NetSink.h.
//
//  Author:  Petr / chiptron.cz   (vyvoj / development: chiptron.cz)
// =============================================================================
#include "NetSink.h"
#include "Config.h"      // NET_BODY_BUDGET_MS
#include <HTTPClient.h>
#include <string.h>      // memcpy / memmove
#include <esp_heap_caps.h>
#include <Stream.h>

// 0.9.2: what the network part is doing right now, for the stuck-loop report.
static const char* s_activity = nullptr;
void Net_SetActivity(const char* what) { s_activity = what; }
const char* Net_Activity() { return s_activity; }

class NetBufSink : public Stream {
public:
  // buf/cap: caller-owned destination. poll: yield + feed the watchdog, called
  // on every block that arrives. budgetMs: 0 disables the time limit.
  NetBufSink(uint8_t* buf, size_t cap, void (*poll)() = nullptr,
             uint32_t budgetMs = 0);

  // --- Print ---
  size_t write(uint8_t b) override;
  size_t write(const uint8_t* data, size_t len) override;

  // --- Stream: this sink is write-only, these are never used. ---
  int  available() override { return 0; }
  int  read() override { return -1; }
  int  peek() override { return -1; }
  void flush() override {}

  size_t length()     const { return _len; }
  bool   overflowed() const { return _over; }
  bool   timedOut()   const { return _timeout; }

  // NUL-terminates the buffer so the result can be treated as a C string.
  // Returns false when there is no room left for the terminator.
  bool   terminate();

private:
  uint8_t*  _buf;
  size_t    _cap;
  size_t    _len     = 0;
  void    (*_poll)();
  uint32_t  _budget;
  uint32_t  _start;
  bool      _over    = false;
  bool      _timeout = false;
public:
  NetworkClient* conn = nullptr;   // 0.9.2: cut when the budget runs out
};

class NetScanSink : public Stream {
public:
  NetScanSink(NetScanFn cb, void* user, void (*poll)() = nullptr,
              uint32_t budgetMs = 0);

  // --- Print ---
  size_t write(uint8_t b) override;
  size_t write(const uint8_t* data, size_t len) override;

  // --- Stream: write-only sink, never read from. ---
  int  available() override { return 0; }
  int  read() override { return -1; }
  int  peek() override { return -1; }
  void flush() override {}

  // Scan whatever is left in the window. Call once the transfer is over.
  void finish();

  size_t length()   const { return _total; }
  bool   timedOut() const { return _timeout; }

private:
  void scanWindow();
  static const size_t WIN     = 512;
  static const size_t OVERLAP = NET_SCAN_MAX_TOKEN;

  NetScanFn _cb;
  void*     _user;
  void    (*_poll)();
  uint32_t  _budget;
  uint32_t  _start;
  char      _win[WIN + 1];
  size_t    _fill    = 0;
  size_t    _total   = 0;
  bool      _timeout = false;
public:
  NetworkClient* conn = nullptr;   // 0.9.2: cut when the budget runs out
};

// 0.9.2: what a body read does when its budget runs out.
//
// A short write stops writeToStreamDataBlock() only when some bytes came in.
// When the server (or a connection that died half-way - a router restart, a
// WiFi drop) sends NOTHING, readBytes() returns 0 after the read timeout, the
// sink is asked to write 0 bytes, 0 == 0 counts as success, and the loop goes
// round again for as long as the socket claims to be connected - which for a
// dead peer on a receive-only socket is forever. Each round fed the watchdog
// through the poll callback, so nothing ever restarted: the screen stood still
// and the touch did nothing. Closing the socket is the one thing that ends the
// loop - connected() goes false and writeToStream() returns an error.
static void cutOff(NetworkClient* c) {
  if (c) c->stop();
}

NetBufSink::NetBufSink(uint8_t* buf, size_t cap, void (*poll)(), uint32_t budgetMs)
  : _buf(buf), _cap(buf ? cap : 0), _poll(poll), _budget(budgetMs),
    _start(millis()) {}

size_t NetBufSink::write(uint8_t b) {
  return write(&b, 1);
}

size_t NetBufSink::write(const uint8_t* data, size_t len) {
  if (_poll) _poll();          // yield + feed the watchdog on every block

  // A short write is how a Stream tells writeToStreamDataBlock() to stop: it
  // retries once, then bails out with HTTPC_ERROR_STREAM_WRITE. That is the
  // only lever we have, since the block loop never asks us anything else.
  if (_budget && (millis() - _start) > _budget) {
    if (!_timeout) cutOff(conn);     // see cutOff() below
    _timeout = true;
    return 0;
  }
  if (!_buf || len == 0) return 0;

  size_t room = (_len < _cap) ? (_cap - _len) : 0;
  if (len > room) {
    _over = true;              // record it - the caller must not use the body
    len   = room;
  }
  if (len) {
    memcpy(_buf + _len, data, len);
    _len += len;
  }
  return len;
}

bool NetBufSink::terminate() {
  if (!_buf || _len >= _cap) return false;
  _buf[_len] = '\0';
  return true;
}

long Net_ReadBody(HTTPClient& http, uint8_t* buf, size_t cap, const char* tag,
                  void (*poll)()) {
  if (!buf || cap == 0) return -1;

  // Reserve one byte so the body can always be NUL-terminated.
  NetBufSink sink(buf, cap - 1, poll, NET_BODY_BUDGET_MS);
  sink.conn = http.getStreamPtr();
  Net_SetActivity(tag);
  int ret = http.writeToStream(&sink);
  Net_SetActivity(nullptr);

  if (sink.timedOut()) {
    Serial.printf("%s: prenos prekrocil %lu ms, preruseno\n",
                  tag, (unsigned long)NET_BODY_BUDGET_MS);
    return -1;
  }
  if (sink.overflowed()) {
    // Deliberately fatal. A body that outgrew the buffer is a truncated body,
    // and half a JSON document or half a PNG is worse than no update at all.
    Serial.printf("%s: odpoved presahla %u B, zahozeno\n", tag, (unsigned)cap);
    return -1;
  }
  if (ret < 0) {
    Serial.printf("%s: writeToStream chyba %d (%s)\n", tag, ret,
                  HTTPClient::errorToString(ret).c_str());
    return -1;
  }
  if (!sink.terminate()) {
    Serial.printf("%s: neni misto pro ukonceni retezce\n", tag);
    return -1;
  }
  return (long)sink.length();
}

NetScanSink::NetScanSink(NetScanFn cb, void* user, void (*poll)(),
                         uint32_t budgetMs)
  : _cb(cb), _user(user), _poll(poll), _budget(budgetMs), _start(millis()) {}

size_t NetScanSink::write(uint8_t b) {
  return write(&b, 1);
}

size_t NetScanSink::write(const uint8_t* data, size_t len) {
  if (_poll) _poll();          // yield + feed the watchdog on every block

  if (_budget && (millis() - _start) > _budget) {
    if (!_timeout) cutOff(conn);           // see cutOff(): a short write alone
    _timeout = true;                       // does not stop a silent connection
    return 0;
  }
  if (!data || len == 0) return 0;

  // Accepts everything - the window is drained as it fills, so the body can be
  // any size.
  size_t done = 0;
  while (done < len) {
    size_t room = WIN - _fill;
    size_t take = (len - done < room) ? (len - done) : room;
    memcpy(_win + _fill, data + done, take);
    _fill += take;
    done  += take;
    if (_fill == WIN) scanWindow();
  }
  _total += len;
  return len;
}

void NetScanSink::scanWindow() {
  _win[_fill] = '\0';
  if (_cb) _cb(_win, _user);
  // Carry the tail so a token split across two windows is still seen whole.
  if (_fill > OVERLAP) {
    memmove(_win, _win + _fill - OVERLAP, OVERLAP);
    _fill = OVERLAP;
  }
}

void NetScanSink::finish() {
  if (!_fill) return;
  _win[_fill] = '\0';
  if (_cb) _cb(_win, _user);
  _fill = 0;
}

long Net_ScanBody(HTTPClient& http, NetScanFn cb, void* user, const char* tag,
                  void (*poll)()) {
  if (!cb) return -1;

  NetScanSink sink(cb, user, poll, NET_BODY_BUDGET_MS);
  sink.conn = http.getStreamPtr();
  Net_SetActivity(tag);
  int ret = http.writeToStream(&sink);
  Net_SetActivity(nullptr);
  sink.finish();               // the last, partial window

  if (sink.timedOut()) {
    Serial.printf("%s: prenos prekrocil %lu ms, preruseno\n",
                  tag, (unsigned long)NET_BODY_BUDGET_MS);
    return -1;
  }
  if (ret < 0) {
    Serial.printf("%s: writeToStream chyba %d (%s)\n", tag, ret,
                  HTTPClient::errorToString(ret).c_str());
    return -1;
  }
  return (long)sink.length();
}

bool Net_HeapOk(const char* tag) {
  size_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (freeInt >= NET_MIN_HEAP) return true;
  Serial.printf("%s: malo volne pameti (%u B < %u B), stahovani preskoceno\n",
                tag, (unsigned)freeInt, (unsigned)NET_MIN_HEAP);
  return false;
}
