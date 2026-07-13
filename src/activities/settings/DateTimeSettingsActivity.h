#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class DateTimeSettingsActivity final : public Activity {
 public:
  explicit DateTimeSettingsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DateTimeSettings", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;
  int visibleItemCount = 0;

  void handleSelection();
};
