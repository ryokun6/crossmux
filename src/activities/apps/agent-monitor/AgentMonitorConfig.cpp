#include "AgentMonitorConfig.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>
#include <ObfuscationUtils.h>

namespace {
constexpr char kConfigPath[] = "/.crosspoint/agent-monitor.json";
constexpr size_t kMaxHostLength = 127;
constexpr size_t kMaxUsernameLength = 63;
constexpr size_t kMaxPasswordLength = 127;
constexpr size_t kMaxTopicLength = 127;

std::string bounded(const char* value, size_t maxLength) {
  std::string result = value ? value : "";
  if (result.size() > maxLength) result.resize(maxLength);
  return result;
}
}  // namespace

AgentMonitorConfig& AgentMonitorConfig::getInstance() {
  static AgentMonitorConfig instance;
  return instance;
}

void AgentMonitorConfig::resetDefaults() {
  host = kDefaultHost;
  port = kDefaultPort;
  username.clear();
  password.clear();
  topic = kDefaultTopic;
}

bool AgentMonitorConfig::load() {
  resetDefaults();
  if (!Storage.exists(kConfigPath)) return false;

  const String json = Storage.readFile(kConfigPath);
  if (json.isEmpty()) return false;

  JsonDocument doc;
  const DeserializationError error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("AGCFG", "Invalid config JSON: %s", error.c_str());
    return false;
  }

  setHost(doc["host"] | kDefaultHost);
  setPort(doc["port"] | kDefaultPort);
  setUsername(doc["username"] | "");
  setTopic(doc["topic"] | kDefaultTopic);

  const char* encodedPassword = doc["password"] | "";
  if (encodedPassword[0]) {
    bool decoded = false;
    password = obfuscation::deobfuscateFromBase64(encodedPassword, &decoded);
    if (!decoded) {
      password.clear();
      LOG_ERR("AGCFG", "Could not decode MQTT password");
    }
  }
  return true;
}

bool AgentMonitorConfig::save() const {
  JsonDocument doc;
  doc["version"] = 1;
  doc["host"] = host;
  doc["port"] = port;
  doc["username"] = username;
  doc["password"] = obfuscation::obfuscateToBase64(password);
  doc["topic"] = topic;

  String json;
  if (serializeJsonPretty(doc, json) == 0) return false;
  Storage.mkdir("/.crosspoint");
  const bool saved = Storage.writeFile(kConfigPath, json);
  if (!saved) LOG_ERR("AGCFG", "Failed to save %s", kConfigPath);
  return saved;
}

void AgentMonitorConfig::setHost(const std::string& value) {
  host = bounded(value.c_str(), kMaxHostLength);
  if (host.empty()) host = kDefaultHost;
}

void AgentMonitorConfig::setPort(uint16_t value) { port = value == 0 ? kDefaultPort : value; }

void AgentMonitorConfig::setUsername(const std::string& value) {
  username = bounded(value.c_str(), kMaxUsernameLength);
}

void AgentMonitorConfig::setPassword(const std::string& value) {
  password = bounded(value.c_str(), kMaxPasswordLength);
}

void AgentMonitorConfig::setTopic(const std::string& value) {
  topic = bounded(value.c_str(), kMaxTopicLength);
  if (topic.empty()) topic = kDefaultTopic;
}
