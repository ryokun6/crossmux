#include "AgentMonitorSettingsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdlib>

#include "../../../components/UITheme.h"
#include "../../../fontIds.h"
#include "../../ActivityManager.h"
#include "../../util/KeyboardEntryActivity.h"
#include "AgentMonitorConfig.h"

namespace {
const StrId kMenuNames[AgentMonitorSettingsActivity::kMenuItems] = {
    StrId::STR_AGENT_MQTT_HOST,     StrId::STR_AGENT_MQTT_PORT,  StrId::STR_AGENT_MQTT_USERNAME,
    StrId::STR_AGENT_MQTT_PASSWORD, StrId::STR_AGENT_MQTT_TOPIC, StrId::STR_AGENT_MQTT_SAVE_RECONNECT,
    StrId::STR_AGENT_MQTT_RESET,
};
}  // namespace

void AgentMonitorSettingsActivity::onEnter() {
  Activity::onEnter();
  AGENT_MONITOR_CONFIG.load();
  selectedIndex = 0;
  requestUpdate();
}

void AgentMonitorSettingsActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    handleSelection();
    return;
  }
  buttonNavigator.onNext([this] {
    selectedIndex = (selectedIndex + 1) % kMenuItems;
    requestUpdate();
  });
  buttonNavigator.onPrevious([this] {
    selectedIndex = (selectedIndex + kMenuItems - 1) % kMenuItems;
    requestUpdate();
  });
}

void AgentMonitorSettingsActivity::handleSelection() {
  if (selectedIndex == 0) {
    openKeyboard(tr(STR_AGENT_MQTT_HOST), AGENT_MONITOR_CONFIG.getHost(), 127, InputType::Url,
                 [](const ActivityResult& result) {
                   if (result.isCancelled) return;
                   AGENT_MONITOR_CONFIG.setHost(std::get<KeyboardResult>(result.data).text);
                   AGENT_MONITOR_CONFIG.save();
                 });
  } else if (selectedIndex == 1) {
    editPort();
  } else if (selectedIndex == 2) {
    openKeyboard(tr(STR_AGENT_MQTT_USERNAME), AGENT_MONITOR_CONFIG.getUsername(), 63, InputType::Text,
                 [](const ActivityResult& result) {
                   if (result.isCancelled) return;
                   AGENT_MONITOR_CONFIG.setUsername(std::get<KeyboardResult>(result.data).text);
                   AGENT_MONITOR_CONFIG.save();
                 });
  } else if (selectedIndex == 3) {
    openKeyboard(tr(STR_AGENT_MQTT_PASSWORD), AGENT_MONITOR_CONFIG.getPassword(), 127, InputType::Password,
                 [](const ActivityResult& result) {
                   if (result.isCancelled) return;
                   AGENT_MONITOR_CONFIG.setPassword(std::get<KeyboardResult>(result.data).text);
                   AGENT_MONITOR_CONFIG.save();
                 });
  } else if (selectedIndex == 4) {
    openKeyboard(tr(STR_AGENT_MQTT_TOPIC), AGENT_MONITOR_CONFIG.getTopic(), 127, InputType::Text,
                 [](const ActivityResult& result) {
                   if (result.isCancelled) return;
                   AGENT_MONITOR_CONFIG.setTopic(std::get<KeyboardResult>(result.data).text);
                   AGENT_MONITOR_CONFIG.save();
                 });
  } else if (selectedIndex == 5) {
    AGENT_MONITOR_CONFIG.save();
    finish();
  } else {
    AGENT_MONITOR_CONFIG.resetDefaults();
    AGENT_MONITOR_CONFIG.save();
    requestUpdate();
  }
}

void AgentMonitorSettingsActivity::editPort() {
  openKeyboard(tr(STR_AGENT_MQTT_PORT), std::to_string(AGENT_MONITOR_CONFIG.getPort()), 5, InputType::Text,
               [](const ActivityResult& result) {
                 if (result.isCancelled) return;
                 const std::string& text = std::get<KeyboardResult>(result.data).text;
                 char* end = nullptr;
                 const unsigned long value = std::strtoul(text.c_str(), &end, 10);
                 if (end != text.c_str() && *end == '\0' && value > 0 && value <= 65535) {
                   AGENT_MONITOR_CONFIG.setPort(static_cast<uint16_t>(value));
                   AGENT_MONITOR_CONFIG.save();
                 }
               });
}

void AgentMonitorSettingsActivity::openKeyboard(const char* title, const std::string& initialText, size_t maxLength,
                                                InputType inputType, ActivityResultHandler handler) {
  auto keyboard =
      makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, title, initialText, maxLength, inputType);
  if (!keyboard) {
    LOG_ERR("AGMON", "OOM opening MQTT keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), std::move(handler));
}

void AgentMonitorSettingsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_AGENT_MQTT_SETTINGS));
  GUI.drawSubHeader(renderer, Rect{0, metrics.topPadding + metrics.headerHeight, pageWidth, metrics.tabBarHeight},
                    tr(STR_AGENT_MQTT_SETTINGS_HINT));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.tabBarHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  GUI.drawList(
      renderer, Rect{0, contentTop, pageWidth, contentHeight}, static_cast<int>(kMenuItems),
      static_cast<int>(selectedIndex), [](int index) { return std::string(I18N.get(kMenuNames[index])); }, nullptr,
      nullptr,
      [](int index) {
        if (index == 0)
          return AGENT_MONITOR_CONFIG.getHost().empty() ? std::string(tr(STR_NOT_SET)) : AGENT_MONITOR_CONFIG.getHost();
        if (index == 1) return std::to_string(AGENT_MONITOR_CONFIG.getPort());
        if (index == 2)
          return AGENT_MONITOR_CONFIG.getUsername().empty() ? std::string(tr(STR_NOT_SET))
                                                            : AGENT_MONITOR_CONFIG.getUsername();
        if (index == 3)
          return AGENT_MONITOR_CONFIG.getPassword().empty() ? std::string(tr(STR_NOT_SET)) : std::string("******");
        if (index == 4) return AGENT_MONITOR_CONFIG.getTopic();
        return std::string();
      },
      true);

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
