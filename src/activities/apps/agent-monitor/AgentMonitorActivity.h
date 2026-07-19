#pragma once

#include <PubSubClient.h>
#include <WiFi.h>

#include <cstdint>

#include "../../Activity.h"

class AgentMonitorActivity final : public Activity {
 public:
  AgentMonitorActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AgentMonitor", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override { return true; }

  void onMqttPayload(const uint8_t* payload, size_t length);

 private:
  static constexpr uint8_t kMaxAgents = 12;
  static constexpr uint8_t kAgentsPerPage = 4;
  static constexpr uint32_t kWifiReconnectMs = 5000;
  static constexpr uint32_t kMqttReconnectMinMs = 5000;
  static constexpr uint32_t kMqttReconnectMaxMs = 30000;
  static constexpr uint32_t kMinRefreshMs = 5000;
  static constexpr uint32_t kOfflineMs = 15000;

  struct AgentRow {
    char type[12]{};
    char status[24]{};
    char project[40]{};
    char tool[28]{};
    char summary[88]{};
    uint32_t elapsed = 0;
    uint32_t updatedAt = 0;
  };

  bool startWifi();
  bool connectMqtt();
  bool configureMqttServer();
  void configureMqtt();
  void pumpNetwork();
  void updateNetworkUiState();
  void teardownNetwork();
  void sortAgents();
  void refreshForInput();
  void changePage(int8_t delta);
  uint8_t pageCount() const;
  void drawHeader(int sw);
  void drawSystemMetrics(int y, int sw);
  void drawAgentCard(const AgentRow& agent, int y, int width, bool selected);
  const char* agentName(const char* type) const;
  const char* statusLabel(const char* status) const;
  const char* networkLabel() const;

  WiFiClient mqttNet;
  PubSubClient mqtt{mqttNet};
  AgentRow agents[kMaxAgents]{};
  uint8_t agentCount = 0;
  uint8_t selected = 0;
  uint8_t currentPage = 0;
  uint16_t cpu = 0;
  uint16_t memory = 0;
  uint16_t disk = 0;
  uint32_t lastPayloadMs = 0;
  uint32_t lastWifiAttemptMs = 0;
  uint32_t lastMqttAttemptMs = 0;
  uint32_t mqttReconnectMs = kMqttReconnectMinMs;
  uint32_t lastRefreshMs = 0;
  bool wifiStarted = false;
  bool payloadDirty = true;
  bool mqttOnline = false;
  bool displayedWifiOnline = false;
  bool displayedMqttOnline = false;
  bool mdnsStarted = false;
};
