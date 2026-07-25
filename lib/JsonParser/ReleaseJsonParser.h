#pragma once

#include <cstddef>
#include <cstdint>

#include "StreamingJsonParser.h"

class ReleaseJsonParser {
 public:
  explicit ReleaseJsonParser(const char* firmwareAssetName = "firmware.bin");

  ReleaseJsonParser(const ReleaseJsonParser&) = delete;
  ReleaseJsonParser& operator=(const ReleaseJsonParser&) = delete;

  void reset();
  void feed(const char* data, size_t len);

  bool foundTag() const;
  bool foundFirmware() const;
  const char* getTagName() const;
  const char* getFirmwareUrl() const;
  size_t getFirmwareSize() const;

  // GitHub publishes a per-asset "digest": "sha256:<64 hex>". True only when the
  // matched firmware asset carried one with that exact shape; a missing, wrongly
  // prefixed, wrong-length or non-hex value leaves this false so callers can
  // fail closed rather than flash unverified bytes.
  static constexpr size_t DIGEST_SIZE = 32;
  bool foundFirmwareDigest() const;
  // DIGEST_SIZE raw bytes. Only meaningful when foundFirmwareDigest() is true.
  const uint8_t* getFirmwareDigest() const;

 private:
  enum class Position : uint8_t {
    TOP_LEVEL,
    IN_ASSETS_ARRAY,
    IN_ASSET_OBJECT,
  };

  enum class LastKey : uint8_t {
    NONE,
    TAG_NAME,
    ASSETS,
    ASSET_NAME,
    ASSET_URL,
    ASSET_SIZE,
    ASSET_DIGEST,
  };

  static void sOnKey(void* ctx, const char* key, size_t len);
  static void sOnString(void* ctx, const char* value, size_t len);
  static void sOnNumber(void* ctx, const char* value, size_t len);
  static void sOnBool(void* ctx, bool value);
  static void sOnNull(void* ctx);
  static void sOnObjectStart(void* ctx);
  static void sOnObjectEnd(void* ctx);
  static void sOnArrayStart(void* ctx);
  static void sOnArrayEnd(void* ctx);

  void commitAsset();

  char firmwareAssetName[32];
  StreamingJsonParser parser;

  Position position;
  LastKey lastKey;
  uint8_t depth;
  uint8_t assetDepth;

  char tagName[32];
  char firmwareUrl[512];
  size_t firmwareSize;
  bool tagFound;
  bool firmwareFound;
  // Fixed-size digest storage: 32 bytes inline in the parser, no heap.
  uint8_t firmwareDigest[DIGEST_SIZE];
  bool firmwareDigestFound;

  char currentAssetName[32];
  char currentAssetUrl[512];
  size_t currentAssetSize;
  uint8_t currentAssetDigest[DIGEST_SIZE];
  bool currentAssetDigestFound;
};
