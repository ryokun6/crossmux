#pragma once

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class DateTimeEditActivity final : public Activity {
 public:
  explicit DateTimeEditActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("DateTimeEdit", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selectedField = 0;
  int year = 2026;
  unsigned month = 1;
  unsigned day = 1;
  unsigned hour = 0;
  unsigned minute = 0;

  void adjustSelectedField(int delta);
  void saveAndFinish();
};
