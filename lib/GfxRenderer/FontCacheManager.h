#pragma once

#include <EpdFontFamily.h>

#include <cstdint>
#include <map>
#include <string>

class FontDecompressor;
class SdCardFont;

class FontCacheManager {
 public:
  FontCacheManager(const std::map<int, EpdFontFamily>& fontMap, const std::map<int, SdCardFont*>& sdCardFonts);

  void setFontDecompressor(FontDecompressor* d);

  void clearCache();
  void prewarmCache(int fontId, const char* utf8Text, uint8_t styleMask = 0x0F, bool addRegularFallback = true);
  void logStats(const char* label = "render");
  void resetStats();
  bool canIdlePrewarm(int fontId) const;
  bool needsPrewarmScan(int fontId) const;

  // Scan-mode API: called by GfxRenderer::drawText() during scan pass
  bool isScanning() const;
  void recordText(const char* text, int fontId, EpdFontFamily::Style style);
#ifdef ENABLE_CHINESE_VERSION
  void reportMissingChineseCodepoint(int fontId, uint32_t codepoint);
  uint32_t consumeMissingChineseCodepoint();
#endif

  // The FontDecompressor pointer, needed by GfxRenderer::getGlyphBitmap()
  FontDecompressor* getDecompressor() const { return fontDecompressor_; }

  // RAII scope for two-pass prewarm pattern
  class PrewarmScope {
   public:
    explicit PrewarmScope(FontCacheManager& manager);
    ~PrewarmScope();
    void endScanAndPrewarm();
    PrewarmScope(PrewarmScope&& other) noexcept;
    PrewarmScope& operator=(PrewarmScope&&) = delete;
    PrewarmScope(const PrewarmScope&) = delete;
    PrewarmScope& operator=(const PrewarmScope&) = delete;

   private:
    FontCacheManager* manager_;
    bool active_ = true;
  };
  PrewarmScope createPrewarmScope();

 private:
  const std::map<int, EpdFontFamily>& fontMap_;
  const std::map<int, SdCardFont*>& sdCardFonts_;
  FontDecompressor* fontDecompressor_ = nullptr;

  // Fill scanStyleFace_ from the scanned font's available faces.
  void resolveScanStyleFaces(int fontId);

  enum class ScanMode : uint8_t { None, Scanning };
  ScanMode scanMode_ = ScanMode::None;
  std::string scanText_;
  // Text recorded per non-regular FACE (index 1..3; 0 is unused because regular
  // always prewarms the whole page). Lets bold/italic size their glyph bitmaps to
  // the runs that actually use them instead of the entire page.
  std::string scanStyledText_[4];
  uint32_t scanStyleCounts_[4] = {};
  // Requested style -> index of the face that draws it. Identity for a font with
  // all four faces; collapses e.g. bold-italic onto bold when the family has no
  // bold-italic face, so both runs land in one bucket and get prewarmed together.
  uint8_t scanStyleFace_[4] = {0, 1, 2, 3};
  int scanFontId_ = -1;
#ifdef ENABLE_CHINESE_VERSION
  uint32_t missingChineseCodepoint_ = 0;
#endif
};
