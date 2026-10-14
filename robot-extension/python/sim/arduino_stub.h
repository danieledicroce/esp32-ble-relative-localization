// Finto Arduino/ESP32 BLE per compilare gli sketch su PC: contiene solo cio' che gli sketch usano.
#pragma once
#include <string>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <cstring>

class String : public std::string {
 public:
  String() {}
  String(const std::string &s) : std::string(s) {}
  String(const char *p, size_t n) : std::string(p, n) {}
};

struct SerialStub {
  std::string out;                       // tutto cio' che lo sketch stampa sulla seriale
  void begin(int) {}
  void print(const char *s) { out += s; }
  void println(const char *s) { out += s; out += "\n"; }
  void printf(const char *format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    out += buffer;
  }
};
static SerialStub Serial;

struct BLEAdvertisedDevice {
  std::string manufacturerData;
  int rssi = 0;
  bool haveManufacturerData() { return true; }
  String getManufacturerData() { return String(manufacturerData); }
  int getRSSI() { return rssi; }
};

struct BLEAdvertisedDeviceCallbacks {
  virtual void onResult(BLEAdvertisedDevice advertisedDevice) = 0;
  virtual ~BLEAdvertisedDeviceCallbacks() {}
};

struct BLEAdvertisementData {
  std::string data;
  void setManufacturerData(String s) { data = s; }
};

struct BLEAdvertising {
  std::string current;                   // payload attualmente trasmesso
  void setAdvertisementData(BLEAdvertisementData a) { current = a.data; }
  void start() {}
};

struct BLEScan {
  BLEAdvertisedDeviceCallbacks *callbacks = nullptr;
  void setAdvertisedDeviceCallbacks(BLEAdvertisedDeviceCallbacks *c) { callbacks = c; }
  void setActiveScan(bool) {}
  void start(int, void *, bool) {}
  void stop() {}
  void clearResults() {}
};

struct BLEDevice {
  static void init(const char *) {}
  static BLEAdvertising *getAdvertising() { static BLEAdvertising a; return &a; }
  static BLEScan *getScan() { static BLEScan s; return &s; }
};

inline void delay(int) {}                // il tempo e' gestito dal simulatore (un ciclo = 3 s)
