#pragma once

class GfxRenderer;

/// Drops the resident SD reader font for the duration of a network session and
/// restores the user's selection afterwards.
///
/// Why every Wi‑Fi/TLS screen needs this: a loaded SD .cpfont pins roughly 75KB
/// of the ~224KB internal heap in kern-class tables, ligature pairs, interval
/// tables and per-style mini glyph bitmaps that live for as long as the font is
/// registered. That is enough to starve the Wi‑Fi driver, lwIP's ~5.7KB TCP send
/// buffer and mbedTLS's per-record buffer at the same time — an X3 serving HTTP
/// with one loaded idles at MaxAlloc~2KB, and KOReaderSyncClient's 40KB
/// free-heap TLS gate (KOReaderSyncClient.cpp:39) cannot pass at all.
///
/// Hold one as an Activity member and emplace it in onEnter() (never in the
/// constructor: the replacing activity is constructed while the outgoing one is
/// still rendering, and that one may still be drawing reader text). Two
/// properties of the activity lifecycle make the automatic restore correct:
///  - the destructor runs inside ActivityManager::exitActivity(), which holds the
///    RenderLock across onExit() and the delete, so the SD read that reloads the
///    font cannot race the e-ink SPI bus;
///  - silentRestart() in onExit() never returns, so on reboot paths the reload is
///    skipped entirely and the fresh boot's sdFontSystem.begin() does it instead.
///
/// A no-op when the user has no SD reader font selected: both unloadAll() and
/// ensureLoaded() return immediately when nothing is loaded and nothing is
/// wanted.
class ScopedSdFontUnload {
 public:
  explicit ScopedSdFontUnload(GfxRenderer& renderer);
  ~ScopedSdFontUnload();

  ScopedSdFontUnload(const ScopedSdFontUnload&) = delete;
  ScopedSdFontUnload& operator=(const ScopedSdFontUnload&) = delete;

 private:
  GfxRenderer& renderer_;
};
