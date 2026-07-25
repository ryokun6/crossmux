#pragma once

#include <cstdint>
#include <string>

class AgentMonitorConfig {
 public:
  static constexpr const char* kDefaultHost = "";
  static constexpr uint16_t kDefaultPort = 1883;
  static constexpr const char* kDefaultTopic = "xteink/agent-monitor/state";

  static AgentMonitorConfig& getInstance();

  bool load();
  bool save() const;
  void resetDefaults();

  const std::string& getHost() const { return host; }
  uint16_t getPort() const { return port; }
  const std::string& getUsername() const { return username; }
  const std::string& getPassword() const { return password; }
  const std::string& getTopic() const { return topic; }

  void setHost(const std::string& value);
  void setPort(uint16_t value);
  void setUsername(const std::string& value);
  void setPassword(const std::string& value);
  void setTopic(const std::string& value);

 private:
  AgentMonitorConfig() { resetDefaults(); }

  std::string host;
  uint16_t port = kDefaultPort;
  std::string username;
  std::string password;
  std::string topic;
};

#define AGENT_MONITOR_CONFIG AgentMonitorConfig::getInstance()
