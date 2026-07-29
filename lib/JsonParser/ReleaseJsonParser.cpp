#include "ReleaseJsonParser.h"

#include <cstdlib>
#include <cstring>

namespace {

void safeCopy(char* dst, size_t dstSize, const char* src, size_t srcLen) {
  size_t n = srcLen < dstSize - 1 ? srcLen : dstSize - 1;
  memcpy(dst, src, n);
  dst[n] = '\0';
}

constexpr char DIGEST_PREFIX[] = "sha256:";
constexpr size_t DIGEST_PREFIX_LEN = sizeof(DIGEST_PREFIX) - 1;

int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// "sha256:<2*outSize hex chars>" -> outSize raw bytes. Rejects anything else
// (other algorithms, truncated hex, stray whitespace) so the caller can treat a
// false return as "no usable digest" and fail closed.
bool parseSha256Digest(const char* value, size_t len, uint8_t* out, size_t outSize) {
  if (len != DIGEST_PREFIX_LEN + outSize * 2) return false;
  if (memcmp(value, DIGEST_PREFIX, DIGEST_PREFIX_LEN) != 0) return false;
  const char* hex = value + DIGEST_PREFIX_LEN;
  for (size_t i = 0; i < outSize; i++) {
    const int hi = hexNibble(hex[i * 2]);
    const int lo = hexNibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

}  // namespace

ReleaseJsonParser::ReleaseJsonParser(const char* requestedFirmwareAssetName)
    : parser(JsonCallbacks{this, sOnKey, sOnString, sOnNumber, sOnBool, sOnNull, sOnObjectStart, sOnObjectEnd,
                           sOnArrayStart, sOnArrayEnd}),
      assetCallback(nullptr),
      assetCallbackCtx(nullptr) {
  const char* assetName = requestedFirmwareAssetName != nullptr ? requestedFirmwareAssetName : "firmware.bin";
  safeCopy(firmwareAssetName, sizeof(firmwareAssetName), assetName, strlen(assetName));
  reset();
}

void ReleaseJsonParser::reset() {
  parser.reset();
  position = Position::TOP_LEVEL;
  lastKey = LastKey::NONE;
  depth = 0;
  assetDepth = 0;
  tagName[0] = '\0';
  firmwareUrl[0] = '\0';
  firmwareSize = 0;
  tagFound = false;
  firmwareFound = false;
  memset(firmwareDigest, 0, sizeof(firmwareDigest));
  firmwareDigestFound = false;
  currentAssetName[0] = '\0';
  currentAssetUrl[0] = '\0';
  currentAssetSize = 0;
  memset(currentAssetDigest, 0, sizeof(currentAssetDigest));
  currentAssetDigestFound = false;
}

void ReleaseJsonParser::feed(const char* data, size_t len) { parser.feed(data, len); }

// Deliberately not cleared by reset(): the callback is wiring set up by the owner
// once, while reset() only drops parsed content so the same parser can take a
// second response.
void ReleaseJsonParser::setAssetCallback(const AssetCallback callback, void* ctx) {
  assetCallback = callback;
  assetCallbackCtx = ctx;
}

bool ReleaseJsonParser::foundTag() const { return tagFound; }
bool ReleaseJsonParser::foundFirmware() const { return firmwareFound; }
const char* ReleaseJsonParser::getTagName() const { return tagName; }
const char* ReleaseJsonParser::getFirmwareUrl() const { return firmwareUrl; }
size_t ReleaseJsonParser::getFirmwareSize() const { return firmwareSize; }
bool ReleaseJsonParser::foundFirmwareDigest() const { return firmwareDigestFound; }
const uint8_t* ReleaseJsonParser::getFirmwareDigest() const { return firmwareDigest; }

void ReleaseJsonParser::commitAsset() {
  // Announced before the scratch is cleared, and only for an asset that actually
  // carried a name — a truncated response can close an object that never got one.
  if (assetCallback != nullptr && currentAssetName[0] != '\0') {
    const Asset asset{currentAssetName, currentAssetUrl, currentAssetSize, currentAssetDigest, currentAssetDigestFound};
    assetCallback(assetCallbackCtx, asset);
  }

  if (strcmp(currentAssetName, firmwareAssetName) == 0) {
    memcpy(firmwareUrl, currentAssetUrl, sizeof(firmwareUrl));
    firmwareSize = currentAssetSize;
    memcpy(firmwareDigest, currentAssetDigest, sizeof(firmwareDigest));
    firmwareDigestFound = currentAssetDigestFound;
    firmwareFound = true;
  }
  currentAssetName[0] = '\0';
  currentAssetUrl[0] = '\0';
  currentAssetSize = 0;
  memset(currentAssetDigest, 0, sizeof(currentAssetDigest));
  currentAssetDigestFound = false;
}

// -- SAX callbacks (static trampolines) -------------------------------------

void ReleaseJsonParser::sOnKey(void* ctx, const char* key, size_t len) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth == 1) {
        if (len == 8 && memcmp(key, "tag_name", 8) == 0)
          self->lastKey = LastKey::TAG_NAME;
        else if (len == 6 && memcmp(key, "assets", 6) == 0)
          self->lastKey = LastKey::ASSETS;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    case Position::IN_ASSET_OBJECT:
      if (self->assetDepth == 1) {
        if (len == 4 && memcmp(key, "name", 4) == 0)
          self->lastKey = LastKey::ASSET_NAME;
        else if (len == 20 && memcmp(key, "browser_download_url", 20) == 0)
          self->lastKey = LastKey::ASSET_URL;
        else if (len == 4 && memcmp(key, "size", 4) == 0)
          self->lastKey = LastKey::ASSET_SIZE;
        else if (len == 6 && memcmp(key, "digest", 6) == 0)
          self->lastKey = LastKey::ASSET_DIGEST;
        else
          self->lastKey = LastKey::NONE;
      }
      break;
    default:
      break;
  }
}

void ReleaseJsonParser::sOnString(void* ctx, const char* value, size_t len) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  switch (self->lastKey) {
    case LastKey::TAG_NAME:
      if (self->position == Position::TOP_LEVEL && self->depth == 1) {
        safeCopy(self->tagName, sizeof(self->tagName), value, len);
        self->tagFound = true;
      }
      break;
    case LastKey::ASSET_NAME:
      if (self->position == Position::IN_ASSET_OBJECT && self->assetDepth == 1)
        safeCopy(self->currentAssetName, sizeof(self->currentAssetName), value, len);
      break;
    case LastKey::ASSET_URL:
      if (self->position == Position::IN_ASSET_OBJECT && self->assetDepth == 1)
        safeCopy(self->currentAssetUrl, sizeof(self->currentAssetUrl), value, len);
      break;
    case LastKey::ASSET_DIGEST:
      if (self->position == Position::IN_ASSET_OBJECT && self->assetDepth == 1) {
        self->currentAssetDigestFound =
            parseSha256Digest(value, len, self->currentAssetDigest, sizeof(self->currentAssetDigest));
      }
      break;
    default:
      break;
  }
  self->lastKey = LastKey::NONE;
}

void ReleaseJsonParser::sOnNumber(void* ctx, const char* value, size_t /*len*/) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  if (self->lastKey == LastKey::ASSET_SIZE && self->position == Position::IN_ASSET_OBJECT && self->assetDepth == 1) {
    self->currentAssetSize = static_cast<size_t>(strtoul(value, nullptr, 10));
  }
  self->lastKey = LastKey::NONE;
}

void ReleaseJsonParser::sOnBool(void* ctx, bool /*value*/) {
  static_cast<ReleaseJsonParser*>(ctx)->lastKey = LastKey::NONE;
}

void ReleaseJsonParser::sOnNull(void* ctx) { static_cast<ReleaseJsonParser*>(ctx)->lastKey = LastKey::NONE; }

void ReleaseJsonParser::sOnObjectStart(void* ctx) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  switch (self->position) {
    case Position::TOP_LEVEL:
      self->depth++;
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_ASSETS_ARRAY:
      self->position = Position::IN_ASSET_OBJECT;
      self->assetDepth = 1;
      self->currentAssetName[0] = '\0';
      self->currentAssetUrl[0] = '\0';
      self->currentAssetSize = 0;
      // Cleared here too, not just in commitAsset(): every other scratch field is
      // reset on entry, and a digest that outlived its own asset would be the one
      // stale value with a security consequence.
      self->currentAssetDigestFound = false;
      memset(self->currentAssetDigest, 0, sizeof(self->currentAssetDigest));
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_ASSET_OBJECT:
      self->assetDepth++;
      self->lastKey = LastKey::NONE;
      break;
  }
}

void ReleaseJsonParser::sOnObjectEnd(void* ctx) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_ASSET_OBJECT:
      self->assetDepth--;
      if (self->assetDepth == 0) {
        self->commitAsset();
        self->position = Position::IN_ASSETS_ARRAY;
      }
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

void ReleaseJsonParser::sOnArrayStart(void* ctx) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->lastKey == LastKey::ASSETS && self->depth == 1) {
        self->position = Position::IN_ASSETS_ARRAY;
      } else {
        self->depth++;
      }
      self->lastKey = LastKey::NONE;
      break;
    case Position::IN_ASSET_OBJECT:
      self->assetDepth++;
      self->lastKey = LastKey::NONE;
      break;
    default:
      break;
  }
}

void ReleaseJsonParser::sOnArrayEnd(void* ctx) {
  auto* self = static_cast<ReleaseJsonParser*>(ctx);

  switch (self->position) {
    case Position::TOP_LEVEL:
      if (self->depth > 0) self->depth--;
      break;
    case Position::IN_ASSETS_ARRAY:
      self->position = Position::TOP_LEVEL;
      break;
    case Position::IN_ASSET_OBJECT:
      self->assetDepth--;
      self->lastKey = LastKey::NONE;
      break;
  }
}
