#pragma once

#include "../../../util/ButtonNavigator.h"
#include "../../Activity.h"

enum class InputType;

class AgentMonitorSettingsActivity final : public Activity {
 public:
  AgentMonitorSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("AgentMonitorSettings", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

  static constexpr size_t kMenuItems = 7;

 private:
  void handleSelection();
  void editPort();
  void openKeyboard(const char* title, const std::string& initialText, size_t maxLength, InputType inputType,
                    ActivityResultHandler handler);

  ButtonNavigator buttonNavigator;
  size_t selectedIndex = 0;
};
