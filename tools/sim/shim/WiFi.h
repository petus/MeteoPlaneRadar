#pragma once
#define WL_CONNECTED 3
struct WiFiShim { int st = WL_CONNECTED; int status() { return st; } };
inline WiFiShim WiFi;
