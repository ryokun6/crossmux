#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "../../Activity.h"
#include "ScopedSdFontUnload.h"
#include "StandbyFace.h"
#include "activities/Activity.h"

struct ActivityResult;

class StandbyActivity final : public Activity {
 public:
  explicit StandbyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Standby", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  void onExit() override;

  // Standby manages its own timer/GPIO light-sleep cycles, so the framework's
  // generic deep-sleep timer stays paused for the activity's lifetime.
  bool preventAutoSleep() override { return true; }
  bool skipLoopDelay() override {
    return syncState_ == SyncState::WifiConnecting || syncState_ == SyncState::ClockSyncing;
  }

 private:
  enum class SyncState : uint8_t {
    Idle,
    Delayed,
    WifiConnecting,
    ClockSyncing,
  };

  enum class DisplayMode : uint8_t {
    Normal,     // Header + battery + face dots + face content
    Immersive,  // Face content only; supported boards light-sleep after 35s idle.
  };

  std::unique_ptr<StandbyFace> currentFace_;
  uint8_t faceIndex_ = 0;
  SyncState syncState_ = SyncState::Idle;
  uint32_t syncStartMs_ = 0;
  DisplayMode mode_ = DisplayMode::Normal;
  uint32_t lastInputMs_ = 0;
  bool inverseMode_ = false;  // Confirm toggles black-bg/white-content. Not persisted.

  // Standby brings up Wi‑Fi itself (NTP) and hosts the AirPage face, which does a
  // TLS fetch and keeps an MQTT session open (AirPageFace.cpp:316). Faces get no
  // GfxRenderer outside render(), so the unload is owned here for the whole
  // screen; no face draws reader body text. Released when Standby exits, which is
  // always user-initiated (preventAutoSleep is true), so the reload never lands
  // in a sleep transition.
  std::optional<ScopedSdFontUnload> fontUnload_;

  void switchFace(int8_t delta);
  void startTimeSync();
  bool trySilentWifiConnect();
  void promptForWifi();
  void onWifiResult(const ActivityResult& result);
  void beginClockSync();
  void pumpTimeSync();
  void stopTimeSyncWifi();
  void completeTimeSync();
  void processFaceTick(bool waitForUpdate);
  bool tryLightSleep(uint32_t idleMs);

  // Layer a 4-level grayscale refresh on top of the BW image just committed by
  // displayBuffer(): re-render the LSB then MSB planes and composite via the
  // gray LUT waveform. Used by passive faces in Immersive mode (gated on
  // wantsGrayscale, e.g. the 老黄历 calendar). The face's render() must be
  // idempotent across the three passes.
  void applyGrayscalePass(const Rect& viewport);
};
