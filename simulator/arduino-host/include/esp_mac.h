#pragma once
// Simulator shim for esp_mac.h. The real firmware reads the per-device eFuse
// MAC; the host has none, so we return a fixed deterministic MAC. Its sole
// consumer is ObfuscationUtils (lib/Serialization), which XORs stored
// credentials (WiFi / OPDS / WeRead) with these 6 bytes — a fixed key keeps
// obfuscated files written by one sim run readable by the next. Note this MAC
// does NOT influence airpage::deviceId(): that id is a random, SD-persisted
// nanoid (see AirPageDeviceId), unrelated to the hardware MAC.
#include <cstdint>
#include <cstring>

inline int esp_efuse_mac_get_default(uint8_t* mac) {
  static const uint8_t kFakeMac[6] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
  if (mac) memcpy(mac, kFakeMac, sizeof(kFakeMac));
  return 0;
}
