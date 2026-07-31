#pragma once

#include <cstdint>

#include "activities/Activity.h"

<<<<<<< HEAD
// Manual one-shot clock sync. If WiFi is not connected yet, it reuses the
=======
// Manual NTP resync action. Runs a forced sync (bypassing the once-per-device debounce),
// reports success/failure, then waits for Back. If WiFi is not connected yet, it reuses the
>>>>>>> upstream/master
// normal WiFi selection flow first.
class ClockSyncActivity final : public Activity {
 public:
  explicit ClockSyncActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ClockSync", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  bool skipLoopDelay() override { return true; }
  void render(RenderLock&&) override;

 private:
  enum class State : uint8_t {
    Syncing,
    Success,
    NoWifi,
    Failed,
  };
  State state = State::Syncing;
  char syncedTime[16] = {0};
  bool shouldTearDownWifiOnExit = false;

  void runSync();
  void launchWifiSelection();
  void onWifiSelectionComplete(bool connected);
};
