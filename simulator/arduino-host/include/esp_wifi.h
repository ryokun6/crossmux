#pragma once
// Host stub for ESP-IDF's WiFi C API. The simulator has no real WiFi; this
// header exists only for linkage of code (e.g. StandbyActivity) that calls
// esp_wifi_deinit() defensively before light sleep on real hardware, and for
// HttpDownloader's WifiPsBoost which toggles modem sleep around large GETs.

typedef enum {
  WIFI_PS_NONE = 0,
  WIFI_PS_MIN_MODEM,
  WIFI_PS_MAX_MODEM,
} wifi_ps_type_t;

inline int esp_wifi_deinit() { return 0; }
inline int esp_wifi_stop() { return 0; }
inline int esp_wifi_set_ps(wifi_ps_type_t) { return 0; }
