#pragma once

#include <cstdint>

#include "../../../util/ButtonNavigator.h"
#include "../../Activity.h"

class WeReadMenuActivity final : public Activity {
 public:
  WeReadMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);
  ~WeReadMenuActivity() override = default;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ButtonNavigator buttonNavigator;
  int selected = 0;

  // Set in onEnter() and refreshed when the user returns from Setup.
  bool wifiOk = false;
  bool keyOk = false;

  // Sticky banner: when set, the next render() shows a popup before drawing
  // the menu. Cleared by any input.
  enum class Banner : uint8_t { None, NoWifi, ComingSoon };
  Banner banner = Banner::None;

  // True once this Activity instance has fired off WifiSelectionActivity for
  // an auto-connect attempt. Prevents re-popping the WiFi UI in a loop if the
  // user cancels. Reset on the next entry (new Activity instance).
  bool autoConnectAttempted_ = false;

  void refreshGates();
  void onSelect();
  void launchAutoConnect();
};
