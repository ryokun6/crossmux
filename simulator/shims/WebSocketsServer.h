#pragma once
// Header stub for WebSocketsServer. Out of scope for simulator.

#include <cstdint>

enum WStype_t {
  WStype_ERROR = 0,
  WStype_DISCONNECTED,
  WStype_CONNECTED,
  WStype_TEXT,
  WStype_BIN,
  WStype_PING,
  WStype_PONG
};

class WebSocketsServer {
 public:
  explicit WebSocketsServer(uint16_t = 81) {}
  void begin() {}
  void loop() {}
};
