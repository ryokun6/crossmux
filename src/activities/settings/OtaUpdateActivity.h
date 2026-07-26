#pragma once

#include "activities/Activity.h"
#include "network/OtaUpdater.h"
#include "util/ButtonNavigator.h"

class OtaUpdateActivity : public Activity {
  enum State {
    WIFI_SELECTION,
    CHECKING_FOR_UPDATE,
    WAITING_CONFIRMATION,
    SKU_SELECTION,       // choosing which language build to write
    SKU_CONFIRMATION,    // spelling out what the chosen build changes
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

  ButtonNavigator buttonNavigator;
  // The SKUs this release actually publishes, as Sku values in enum order. A
  // release predating a language simply has no asset for it, so the list is built
  // from the response rather than from the enum.
  uint8_t skuRows_[OtaUpdater::SKU_COUNT] = {};
  uint8_t skuRowCount_ = 0;
  int selectedSkuRow_ = 0;
  // Where Back returns to, so the list can be reached from both the "an update is
  // out" and the "you are up to date" screens without either becoming a trap.
  State skuReturnState_ = NO_UPDATE;

  void onWifiSelectionComplete(bool success);
  void setFailure(OtaUpdater::OtaUpdaterError error);
  void buildSkuRows();
  void enterSkuSelection();
  void confirmSkuSelection();
  void runInstall();
  OtaUpdater::Sku rowSku(int row) const { return static_cast<OtaUpdater::Sku>(skuRows_[row]); }
  // Label for a build in the *current* UI language, not in the build's own
  // language: a global-build device has no CJK font, so naming the Japanese image
  // 日本語 would draw a row of empty boxes.
  static const char* skuLabel(OtaUpdater::Sku sku);

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
