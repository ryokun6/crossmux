#pragma once
// Stub: host build has no TLS stack. NetworkClientSecure inherits from WiFiClient
// so HTTPClient::begin(WiFiClient&, ...) accepts a NetworkClientSecure&. All
// methods are no-ops; callers fail earlier at WiFi.status() != WL_CONNECTED.

#include <HTTPClient.h>  // defines WiFiClient

#include <cstdint>

class NetworkClientSecure : public WiFiClient {
 public:
  void setInsecure() {}
  void setCACert(const char*) {}
  void setCACertBundle(const uint8_t*) {}
};
