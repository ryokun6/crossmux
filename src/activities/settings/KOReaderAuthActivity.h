#pragma once

#include <functional>
#include <optional>

#include "ScopedSdFontUnload.h"
#include "activities/Activity.h"

/**
 * Activity for testing KOReader credentials.
 * Connects to WiFi and authenticates with the KOReader sync server.
 */
class KOReaderAuthActivity final : public Activity {
 public:
  explicit KOReaderAuthActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("KOReaderAuth", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return state == CONNECTING || state == AUTHENTICATING; }

 private:
  enum State { WIFI_SELECTION, CONNECTING, AUTHENTICATING, SUCCESS, FAILED };

  State state = WIFI_SELECTION;
  std::string statusMessage;
  std::string errorMessage;
  bool credentialFailure = false;

  // Engaged only once Wi‑Fi is up and the handshake is about to run. onExit's
  // silentRestart() normally reboots before the destructor reloads the font.
  std::optional<ScopedSdFontUnload> fontUnload_;

  void onWifiSelectionComplete(bool success);
  void performAuthentication();
};
