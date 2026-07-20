#include "AgentMonitorActivity.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <esp_wifi.h>

#include <algorithm>
#include <atomic>
#include <cstring>

#include "../../../WifiCredentialStore.h"
#include "../../../components/UITheme.h"
#include "../../../fontIds.h"
#include "AgentMonitorConfig.h"
#include "AgentMonitorSettingsActivity.h"

namespace {

AgentMonitorActivity* s_instance = nullptr;
std::atomic<bool> s_storageScanStarted{false};
std::atomic<bool> s_storageScanReady{false};
uint64_t s_storageFreeBytes = 0;

void storageScanTask(void*) {
  s_storageFreeBytes = Storage.freeBytes();
  s_storageScanReady.store(true, std::memory_order_release);
  vTaskDelete(nullptr);
}

void startStorageScan() {
  if (s_storageScanReady.load(std::memory_order_acquire) || s_storageScanStarted.exchange(true)) return;
  if (xTaskCreate(storageScanTask, "agmon-storage", 3072, nullptr, 1, nullptr) != pdPASS) {
    s_storageScanStarted.store(false);
    LOG_ERR("AGMON", "Could not start storage scan task");
  }
}

// cppcheck-suppress constParameterCallback -- PubSubClient requires a mutable payload callback signature.
void mqttCallback(char*, uint8_t* payload, unsigned int length) {
  if (s_instance) s_instance->onMqttPayload(payload, length);
}

void copyText(char* target, size_t capacity, const char* source) {
  if (!target || capacity == 0) return;
  strlcpy(target, source ? source : "", capacity);
}

bool isActiveStatus(const char* status) {
  return strcmp(status, "waiting_permission") == 0 || strcmp(status, "tool_running") == 0 ||
         strcmp(status, "working") == 0 || strcmp(status, "thinking") == 0;
}

}  // namespace

void AgentMonitorActivity::onEnter() {
  Activity::onEnter();
  s_instance = this;
  AGENT_MONITOR_CONFIG.load();
  configureMqtt();
  lastWifiAttemptMs = millis() - kWifiReconnectMs;
  lastMqttAttemptMs = millis() - kMqttReconnectMinMs;
  mqttReconnectMs = kMqttReconnectMinMs;
  displayedWifiOnline = WiFi.status() == WL_CONNECTED;
  displayedMqttOnline = false;
  storageFreeReady = s_storageScanReady.load(std::memory_order_acquire);
  if (storageFreeReady) storageFreeBytes = s_storageFreeBytes;
  startStorageScan();
  payloadDirty = true;
  requestUpdate();
}

void AgentMonitorActivity::onExit() {
  s_instance = nullptr;
  teardownNetwork();
  Activity::onExit();
}

bool AgentMonitorActivity::startWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (wifiStarted) return false;

  if (WIFI_STORE.getCredentials().empty()) WIFI_STORE.loadFromFile();
  const std::string& last = WIFI_STORE.getLastConnectedSsid();
  const WifiCredential* credential = last.empty() ? nullptr : WIFI_STORE.findCredential(last);
  if (!credential) {
    LOG_ERR("AGMON", "No saved WiFi credential");
    return false;
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (credential->password.empty()) {
    WiFi.begin(credential->ssid.c_str());
  } else {
    WiFi.begin(credential->ssid.c_str(), credential->password.c_str());
  }
  wifiStarted = true;
  return false;
}

void AgentMonitorActivity::configureMqtt() {
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(4096);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(5);
  mqttNet.setTimeout(5000);
}

bool AgentMonitorActivity::configureMqttServer() {
  const std::string& host = AGENT_MONITOR_CONFIG.getHost();
  if (host.empty()) return false;
  constexpr char kLocalSuffix[] = ".local";
  if (host.size() <= strlen(kLocalSuffix) ||
      host.compare(host.size() - strlen(kLocalSuffix), strlen(kLocalSuffix), kLocalSuffix) != 0) {
    mqtt.setServer(host.c_str(), AGENT_MONITOR_CONFIG.getPort());
    return true;
  }

  if (!mdnsStarted) {
    char mdnsName[32];
    snprintf(mdnsName, sizeof(mdnsName), "x4-agent-%08lx", static_cast<unsigned long>(ESP.getEfuseMac()));
    mdnsStarted = MDNS.begin(mdnsName);
    if (!mdnsStarted) {
      LOG_ERR("AGMON", "Could not start mDNS resolver");
      return false;
    }
  }

  const std::string queryName = host.substr(0, host.size() - strlen(kLocalSuffix));
  const IPAddress address = MDNS.queryHost(queryName.c_str(), 3000);
  if (address == IPAddress(0, 0, 0, 0)) {
    LOG_ERR("AGMON", "mDNS lookup failed: %s", host.c_str());
    return false;
  }
  mqtt.setServer(address, AGENT_MONITOR_CONFIG.getPort());
  LOG_INF("AGMON", "Resolved %s to %s", host.c_str(), address.toString().c_str());
  return true;
}

bool AgentMonitorActivity::connectMqtt() {
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "x4-agent-%08lx", static_cast<unsigned long>(ESP.getEfuseMac()));
  if (!configureMqttServer()) return false;
  const bool connected = AGENT_MONITOR_CONFIG.getUsername().empty()
                             ? mqtt.connect(clientId)
                             : mqtt.connect(clientId, AGENT_MONITOR_CONFIG.getUsername().c_str(),
                                            AGENT_MONITOR_CONFIG.getPassword().c_str());
  if (!connected) {
    LOG_ERR("AGMON", "MQTT connect failed: %d target=%s:%u local=%s rssi=%d", mqtt.state(),
            AGENT_MONITOR_CONFIG.getHost().c_str(), AGENT_MONITOR_CONFIG.getPort(), WiFi.localIP().toString().c_str(),
            WiFi.RSSI());
    return false;
  }
  if (!mqtt.subscribe(AGENT_MONITOR_CONFIG.getTopic().c_str(), 1)) {
    mqtt.disconnect();
    LOG_ERR("AGMON", "MQTT subscribe failed");
    return false;
  }
  mqttOnline = true;
  mqttReconnectMs = kMqttReconnectMinMs;
  payloadDirty = true;
  LOG_INF("AGMON", "MQTT connected to %s:%u topic=%s", AGENT_MONITOR_CONFIG.getHost().c_str(),
          AGENT_MONITOR_CONFIG.getPort(), AGENT_MONITOR_CONFIG.getTopic().c_str());
  return true;
}

void AgentMonitorActivity::pumpNetwork() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (mqtt.connected()) mqtt.disconnect();
    mqttOnline = false;
    if (now - lastWifiAttemptMs >= kWifiReconnectMs) {
      lastWifiAttemptMs = now;
      if (wifiStarted) {
        LOG_INF("AGMON", "WiFi reconnect, status=%d", static_cast<int>(WiFi.status()));
        WiFi.disconnect(false);
        delay(50);
      }
      wifiStarted = false;
      startWifi();
    }
    updateNetworkUiState();
    return;
  }

  if (!mqtt.connected()) {
    mqttOnline = false;
    if (now - lastMqttAttemptMs >= mqttReconnectMs) {
      lastMqttAttemptMs = now;
      if (!connectMqtt()) mqttReconnectMs = std::min(mqttReconnectMs * 2, kMqttReconnectMaxMs);
    }
    updateNetworkUiState();
    return;
  }

  if (!mqtt.loop()) {
    mqttOnline = false;
    LOG_ERR("AGMON", "MQTT connection lost: %d", mqtt.state());
  }
  updateNetworkUiState();
}

void AgentMonitorActivity::updateNetworkUiState() {
  const bool wifiOnline = WiFi.status() == WL_CONNECTED;
  const bool mqttConnected = mqttOnline && mqtt.connected();
  if (wifiOnline == displayedWifiOnline && mqttConnected == displayedMqttOnline) return;
  displayedWifiOnline = wifiOnline;
  displayedMqttOnline = mqttConnected;
  payloadDirty = false;
  lastRefreshMs = millis();
  requestUpdate();
}

void AgentMonitorActivity::teardownNetwork() {
  if (mqtt.connected()) mqtt.disconnect();
  mqttOnline = false;
  if (wifiStarted) {
    WiFi.disconnect(false);
    delay(100);
    WiFi.mode(WIFI_OFF);
    esp_wifi_deinit();
  }
  wifiStarted = false;
  if (mdnsStarted) {
    MDNS.end();
    mdnsStarted = false;
  }
}

void AgentMonitorActivity::onMqttPayload(const uint8_t* payload, size_t length) {
  if (!payload || length == 0 || length > 4095) return;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    LOG_ERR("AGMON", "JSON parse failed: %s", error.c_str());
    return;
  }

  cpu = doc["system"]["cpu"].as<uint16_t>();
  memory = doc["system"]["memory"].as<uint16_t>();
  disk = doc["system"]["disk"].as<uint16_t>();
  agentCount = 0;
  for (JsonObject item : doc["agents"].as<JsonArray>()) {
    if (agentCount >= kMaxAgents) break;
    AgentRow& row = agents[agentCount++];
    copyText(row.type, sizeof(row.type), item["type"] | "agent");
    copyText(row.status, sizeof(row.status), item["status"] | "idle");
    copyText(row.project, sizeof(row.project), item["project"] | "");
    copyText(row.tool, sizeof(row.tool), item["tool"] | "");
    copyText(row.summary, sizeof(row.summary), item["summary"] | "");
    row.elapsed = item["elapsed"].as<uint32_t>();
    row.updatedAt = item["updated_at"].as<uint32_t>();
  }
  sortAgents();
  if (selected >= agentCount) selected = 0;
  currentPage = selected / kAgentsPerPage;
  lastPayloadMs = millis();
  payloadDirty = true;
}

void AgentMonitorActivity::loop() {
  pumpNetwork();

  if (!storageFreeReady && s_storageScanReady.load(std::memory_order_acquire)) {
    storageFreeBytes = s_storageFreeBytes;
    storageFreeReady = true;
    refreshForInput();
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    activityManager.goToApps();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    auto settings = makeUniqueNoThrow<AgentMonitorSettingsActivity>(renderer, mappedInput);
    if (!settings) {
      LOG_ERR("AGMON", "OOM opening MQTT settings");
      return;
    }
    startActivityForResult(std::move(settings), [this](const ActivityResult&) {
      if (mqtt.connected()) mqtt.disconnect();
      mqttOnline = false;
      configureMqtt();
      lastMqttAttemptMs = millis() - kMqttReconnectMinMs;
      mqttReconnectMs = kMqttReconnectMinMs;
      updateNetworkUiState();
    });
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    changePage(1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    changePage(-1);
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Down) && agentCount > 0) {
    selected = static_cast<uint8_t>((selected + 1) % agentCount);
    currentPage = selected / kAgentsPerPage;
    refreshForInput();
  } else if (mappedInput.wasPressed(MappedInputManager::Button::Up) && agentCount > 0) {
    selected = static_cast<uint8_t>((selected + agentCount - 1) % agentCount);
    currentPage = selected / kAgentsPerPage;
    refreshForInput();
  }

  const uint32_t now = millis();
  if (payloadDirty && now - lastRefreshMs >= kMinRefreshMs) {
    payloadDirty = false;
    lastRefreshMs = now;
    requestUpdate();
  }
}

void AgentMonitorActivity::sortAgents() {
  std::sort(agents, agents + agentCount, [](const AgentRow& lhs, const AgentRow& rhs) {
    const bool lhsActive = isActiveStatus(lhs.status);
    const bool rhsActive = isActiveStatus(rhs.status);
    if (lhsActive != rhsActive) return lhsActive;
    if (lhs.updatedAt != rhs.updatedAt) return lhs.updatedAt > rhs.updatedAt;
    return strcmp(lhs.type, rhs.type) < 0;
  });
}

uint8_t AgentMonitorActivity::pageCount() const {
  return agentCount == 0 ? 1 : static_cast<uint8_t>((agentCount + kAgentsPerPage - 1) / kAgentsPerPage);
}

void AgentMonitorActivity::refreshForInput() {
  payloadDirty = false;
  lastRefreshMs = millis();
  requestUpdate();
}

void AgentMonitorActivity::changePage(int8_t delta) {
  const uint8_t count = pageCount();
  if (count <= 1) return;
  currentPage = static_cast<uint8_t>((currentPage + count + delta) % count);
  selected = static_cast<uint8_t>(currentPage * kAgentsPerPage);
  if (selected >= agentCount) selected = static_cast<uint8_t>(agentCount - 1);
  refreshForInput();
}

const char* AgentMonitorActivity::agentName(const char* type) const {
  if (strcmp(type, "pi") == 0) return tr(STR_AGENT_MONITOR_PI);
  if (strcmp(type, "claude") == 0) return tr(STR_AGENT_MONITOR_CLAUDE);
  if (strcmp(type, "codex") == 0) return tr(STR_AGENT_MONITOR_CODEX);
  return tr(STR_AGENT_MONITOR_AGENT);
}

const char* AgentMonitorActivity::statusLabel(const char* status) const {
  if (strcmp(status, "thinking") == 0) return tr(STR_AGENT_STATUS_THINKING);
  if (strcmp(status, "working") == 0 || strcmp(status, "tool_running") == 0) return tr(STR_AGENT_STATUS_WORKING);
  if (strcmp(status, "waiting_permission") == 0) return tr(STR_AGENT_STATUS_WAITING);
  if (strcmp(status, "completed") == 0) return tr(STR_AGENT_STATUS_COMPLETED);
  if (strcmp(status, "failed") == 0) return tr(STR_AGENT_STATUS_FAILED);
  if (strcmp(status, "offline") == 0) return tr(STR_AGENT_STATUS_OFFLINE);
  return tr(STR_AGENT_STATUS_IDLE);
}

const char* AgentMonitorActivity::networkLabel() const {
  if (AGENT_MONITOR_CONFIG.getHost().empty()) return tr(STR_AGENT_MQTT_NOT_CONFIGURED);
  if (mqttOnline) return tr(STR_AGENT_MONITOR_ONLINE);
  if (WiFi.status() == WL_CONNECTED) return tr(STR_AGENT_MONITOR_MQTT_RECONNECTING);
  return tr(STR_AGENT_MONITOR_WIFI_RECONNECTING);
}

void AgentMonitorActivity::drawHeader(int sw) {
  renderer.fillRect(0, 0, sw, 58, true);
  renderer.drawText(UI_12_FONT_ID, 18, 18, tr(STR_AGENT_MONITOR_TITLE), false, EpdFontFamily::BOLD);
  const char* network = networkLabel();
  const int networkWidth = renderer.getTextWidth(SMALL_FONT_ID, network);
  renderer.drawText(SMALL_FONT_ID, sw - networkWidth - 18, 21, network, false);
}

void AgentMonitorActivity::drawSystemMetrics(int y, int sw) {
  constexpr int x = 16;
  constexpr int height = 48;
  constexpr int padding = 12;
  const int width = sw - x * 2;

  const uint32_t heapSize = ESP.getHeapSize();
  const uint16_t heapUsedPercent =
      heapSize == 0 ? 0 : static_cast<uint16_t>(100U - (ESP.getFreeHeap() * 100U / heapSize));
  const uint64_t freeTenthsGiB = storageFreeBytes * 10ULL / (1024ULL * 1024ULL * 1024ULL);

  char metrics[128];
  if (storageFreeReady) {
    snprintf(metrics, sizeof(metrics), "MAC: C%u%% M%u%%;  X4: C%u M%u%% SD%llu.%lluG B%u%%", cpu, memory,
             getCpuFrequencyMhz(), heapUsedPercent, static_cast<unsigned long long>(freeTenthsGiB / 10ULL),
             static_cast<unsigned long long>(freeTenthsGiB % 10ULL), powerManager.getBatteryPercentage());
  } else {
    snprintf(metrics, sizeof(metrics), "MAC: C%u%% M%u%%;  X4: C%u M%u%% SD... B%u%%", cpu, memory,
             getCpuFrequencyMhz(), heapUsedPercent, powerManager.getBatteryPercentage());
  }
  const std::string fittedMetrics = renderer.truncatedText(SMALL_FONT_ID, metrics, width - padding * 2);

  renderer.drawRoundedRect(x, y, width, height, 2, 8, true);
  renderer.drawCenteredText(SMALL_FONT_ID, y + 15, fittedMetrics.c_str(), true, EpdFontFamily::BOLD);
}

void AgentMonitorActivity::drawAgentCard(const AgentRow& agent, int y, int width, bool isSelected) {
  constexpr int height = 132;
  renderer.drawRoundedRect(16, y, width - 32, height, isSelected ? 3 : 1, 8, true);
  if (isSelected) renderer.fillRect(16, y, 8, height, true);

  renderer.drawText(UI_12_FONT_ID, 34, y + 16, agentName(agent.type), true, EpdFontFamily::BOLD);
  const char* status = statusLabel(agent.status);
  const int statusWidth = renderer.getTextWidth(UI_10_FONT_ID, status, EpdFontFamily::BOLD);
  renderer.drawText(UI_10_FONT_ID, width - statusWidth - 30, y + 18, status, true, EpdFontFamily::BOLD);

  char meta[88];
  const uint32_t minutes = agent.elapsed / 60;
  const uint32_t seconds = agent.elapsed % 60;
  snprintf(meta, sizeof(meta), "%s%s%s   %02lu:%02lu", agent.project, agent.tool[0] ? " · " : "", agent.tool,
           static_cast<unsigned long>(minutes), static_cast<unsigned long>(seconds));
  renderer.drawText(SMALL_FONT_ID, 34, y + 55, meta, true);

  const std::string summary = renderer.truncatedText(UI_10_FONT_ID, agent.summary, width - 68);
  renderer.drawText(UI_10_FONT_ID, 34, y + 88, summary.c_str(), true);
}

void AgentMonitorActivity::render(RenderLock&&) {
  const int sw = renderer.getScreenWidth();
  const int sh = renderer.getScreenHeight();
  renderer.clearScreen();
  drawHeader(sw);
  drawSystemMetrics(72, sw);

  const bool stale = lastPayloadMs == 0 || millis() - lastPayloadMs > kOfflineMs;
  if (agentCount == 0) {
    renderer.drawCenteredText(UI_12_FONT_ID, sh / 2 - 20,
                              stale ? tr(STR_AGENT_MONITOR_NO_DATA) : tr(STR_AGENT_MONITOR_IDLE), true,
                              EpdFontFamily::BOLD);
  } else {
    int y = 136;
    const uint8_t first = static_cast<uint8_t>(currentPage * kAgentsPerPage);
    const uint8_t visible = std::min<uint8_t>(kAgentsPerPage, agentCount - first);
    for (uint8_t i = 0; i < visible; ++i) {
      const uint8_t index = static_cast<uint8_t>(first + i);
      drawAgentCard(agents[index], y, sw, index == selected);
      y += 140;
    }
  }

  char page[40];
  snprintf(page, sizeof(page), tr(STR_AGENT_MONITOR_PAGE_FMT), currentPage + 1, pageCount());
  renderer.drawText(SMALL_FONT_ID, 18, sh - 36, stale ? tr(STR_AGENT_STATUS_OFFLINE) : tr(STR_AGENT_MONITOR_UPDATED),
                    true);
  renderer.drawCenteredText(SMALL_FONT_ID, sh - 36, page, true, EpdFontFamily::BOLD);
  const char* pageHint = tr(STR_AGENT_MONITOR_PAGE_HINT);
  const int pageHintWidth = renderer.getTextWidth(SMALL_FONT_ID, pageHint);
  renderer.drawText(SMALL_FONT_ID, sw - pageHintWidth - 18, sh - 36, pageHint, true);
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}
