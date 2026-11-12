// An in-memory filesystem with the handful of calls Cards.cpp makes.
#pragma once
#include <Arduino.h>
#include <map>
#include <memory>
#include <set>
#include <vector>
struct SimFs { std::map<std::string, std::string> files; std::set<std::string> dirs; bool mounted = false; };
inline SimFs g_simfs;

class File : public Stream {
  struct St { std::string path; bool dir = false; size_t pos = 0; std::vector<std::string> listing; size_t next = 0; };
  std::shared_ptr<St> st_;
 public:
  File() {}
  static File dirOf(const std::string& p) {
    File f; f.st_ = std::make_shared<St>(); f.st_->path = p; f.st_->dir = true;
    for (auto& kv : g_simfs.files) if (kv.first.rfind(p + "/", 0) == 0) f.st_->listing.push_back(kv.first);
    return f;
  }
  static File fileOf(const std::string& p) { File f; f.st_ = std::make_shared<St>(); f.st_->path = p; return f; }
  explicit operator bool() const { return (bool)st_; }
  const char* name() const { const size_t i = st_->path.rfind('/'); return st_->path.c_str() + (i == std::string::npos ? 0 : i + 1); }
  bool isDirectory() const { return st_ && st_->dir; }
  size_t size() const { return st_ && !st_->dir ? g_simfs.files[st_->path].size() : 0; }
  File openNextFile() { if (!st_ || st_->next >= st_->listing.size()) return File(); return fileOf(st_->listing[st_->next++]); }
  void close() { st_.reset(); }
  using Print::write;
  size_t write(uint8_t c) override { if (!st_ || st_->dir) return 0; g_simfs.files[st_->path].push_back((char)c); return 1; }
  int available() override { return st_ ? (int)(g_simfs.files[st_->path].size() - st_->pos) : 0; }
  int read() override { if (available() <= 0) return -1; return (unsigned char)g_simfs.files[st_->path][st_->pos++]; }
  int peek() override { if (available() <= 0) return -1; return (unsigned char)g_simfs.files[st_->path][st_->pos]; }
};

struct LittleFSShim {
  bool begin(bool formatOnFail = false) { (void)formatOnFail; g_simfs.mounted = true; return true; }
  bool exists(const char* p) { return g_simfs.dirs.count(p) || g_simfs.files.count(p); }
  bool mkdir(const char* p) { g_simfs.dirs.insert(p); return true; }
  bool remove(const char* p) { return g_simfs.files.erase(p) > 0; }
  File open(const char* p, const char* mode = "r") {
    if (mode[0] == 'w') { g_simfs.files[p] = ""; return File::fileOf(p); }
    if (g_simfs.dirs.count(p)) return File::dirOf(p);
    if (g_simfs.files.count(p)) return File::fileOf(p);
    return File();
  }
};
inline LittleFSShim LittleFS;
