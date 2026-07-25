#pragma once

#include "activities/Activity.h"
#include "network/OtaUpdater.h"

class OtaUpdateActivity : public Activity {
  enum State {
    WIFI_SELECTION,
    CHECKING_FOR_UPDATE,
    WAITING_CONFIRMATION,
    UPDATE_IN_PROGRESS,  // downloading the asset to the SD staging file
    VERIFYING,           // hashing the staged file against the published digest
    FLASHING,            // writing the OTA partition
    NO_UPDATE,
    FAILED,
    FINISHED,
    SHUTTING_DOWN
  };

  // Can't initialize this to 0 or the first render doesn't happen
  static constexpr unsigned int UNINITIALIZED_PERCENTAGE = 111;

  State state = WIFI_SELECTION;
  unsigned int lastUpdaterPercentage = UNINITIALIZED_PERCENTAGE;
  OtaUpdater updater;
  bool resumedAfterDefrag_ = false;
  // Points at a tr() string, so it stays valid without owning storage. Null when
  // the failure has no more specific message than "Update failed".
  const char* errorMessage_ = nullptr;

  void onWifiSelectionComplete(bool success);
  void setFailure(OtaUpdater::OtaUpdaterError error);

 public:
  // resumedAfterDefrag: silent-restart resume path — skip the MaxAlloc defrag
  // reboot (already done) and auto-reconnect Wi‑Fi before the update check.
  explicit OtaUpdateActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool resumedAfterDefrag = false)
      : Activity("OtaUpdate", renderer, mappedInput), updater(), resumedAfterDefrag_(resumedAfterDefrag) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override {
    return state == CHECKING_FOR_UPDATE || state == UPDATE_IN_PROGRESS || state == VERIFYING || state == FLASHING;
  }
  bool skipLoopDelay() override { return true; }  // Prevent power-saving mode
};
