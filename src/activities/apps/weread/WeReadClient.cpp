#include "WeReadClient.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <Logging.h>
#include <NetworkClientSecure.h>
#include <WiFi.h>

#include <cstdlib>
#include <memory>

#include "WeReadKeyStore.h"

namespace {

constexpr const char* kGatewayUrl = "https://i.weread.qq.com/api/agent/gateway";
constexpr const char* kSkillVersion = "1.0.3";  // Must match weread-skills/SKILL.md `version:`

int g_lastErrCode = 0;
std::string g_lastUpgradeMessage;

// Stream wrapper that exposes the decoded HTTP response body as a flat byte
// stream — transparently consuming chunked-transfer-encoding framing when
// the server uses it. ArduinoJson reads from this directly, so peak memory
// is just the parsed JsonDocument; we never buffer the whole body.
//
// Operates in two modes selected at construction:
//   - chunked  : reads <hex-size>\r\n<data>\r\n... and feeds out only <data>.
//   - identity : passes the next `contentLength` bytes through unchanged.
//
// Returns -1 from read() on EOF or socket loss; ArduinoJson treats that as
// end-of-input. peek() is implemented to a one-byte lookahead — Stream
// subclasses are expected to support it, and some callers (HTTPClient's own
// utility paths, ArduinoJson edge cases) do call it.
class HttpBodyStream final : public Stream {
 public:
  static constexpr unsigned long kByteWaitMs = 1500;

  HttpBodyStream(Stream& upstream, NetworkClient& net, bool chunked, int contentLength)
      : up_(upstream), net_(net), chunked_(chunked) {
    // Once our read() returns -1, ArduinoJson's caller (Stream::readBytes ->
    // Stream::timedRead) will keep retrying for `_timeout` ms; with default
    // 1000 ms that's a wasted second at end-of-body. Setting our own timeout
    // to 0 makes EOF terminate the read loop immediately.
    setTimeout(0);
    if (chunked_) {
      remaining_ = 0;  // primed on first read by readNextChunkHeader()
    } else {
      remaining_ = contentLength;
      if (remaining_ <= 0) eof_ = true;
    }
  }

  int available() override { return eof_ ? 0 : up_.available(); }

  int read() override {
    if (peeked_ >= 0) {
      const int c = peeked_;
      peeked_ = -1;
      return c;
    }
    return readImpl();
  }

  int peek() override {
    if (peeked_ >= 0) return peeked_;
    peeked_ = readImpl();
    return peeked_;
  }

  size_t write(uint8_t) override { return 0; }
  void flush() override {}

 private:
  int readImpl() {
    if (eof_) return -1;
    if (chunked_ && remaining_ == 0 && !readNextChunkHeader()) {
      eof_ = true;
      return -1;
    }
    if (!waitForByte()) {
      eof_ = true;
      return -1;
    }
    const int c = up_.read();
    if (c < 0) {
      eof_ = true;
      return -1;
    }
    --remaining_;
    if (remaining_ == 0) {
      if (chunked_) {
        consumeUntilLF();  // the \r\n that follows every chunk body
      } else {
        eof_ = true;
      }
    }
    return c;
  }

  bool readNextChunkHeader() {
    char buf[16];
    size_t idx = 0;
    while (idx < sizeof(buf) - 1) {
      if (!waitForByte()) return false;
      const int c = up_.read();
      if (c < 0) return false;
      if (c == '\r') {
        if (!waitForByte()) return false;
        return up_.read() == '\n' && parseSize(buf, idx);
      }
      if (c == ';') {
        consumeUntilLF();  // chunk-extension ("5;foo=bar\r\n")
        return parseSize(buf, idx);
      }
      buf[idx++] = static_cast<char>(c);
    }
    return false;  // header too long — bail rather than overflow buf
  }

  bool parseSize(char* buf, size_t idx) {
    buf[idx] = '\0';
    const long size = std::strtol(buf, nullptr, 16);
    if (size > 0) {
      remaining_ = static_cast<int>(size);
      return true;
    }
    // Terminating "0\r\n" chunk. Trailing headers (rare) and the final empty
    // line are skipped here; http.end() in the caller closes the connection
    // either way, so partial consumption is harmless.
    drainTrailers();
    return false;
  }

  void drainTrailers() {
    while (waitForByte()) {
      const int c = up_.read();
      if (c < 0) return;
      if (c == '\r') {
        if (waitForByte()) up_.read();  // best-effort consume '\n'
        return;
      }
      consumeUntilLF();
    }
  }

  void consumeUntilLF() {
    while (waitForByte()) {
      const int c = up_.read();
      if (c < 0 || c == '\n') return;
    }
  }

  bool waitForByte() {
    if (up_.available() > 0) return true;
    const unsigned long deadline = millis() + kByteWaitMs;
    while (up_.available() == 0) {
      if (!net_.connected() && up_.available() == 0) return false;
      if (static_cast<long>(millis() - deadline) >= 0) return false;
      delay(1);
    }
    return true;
  }

  Stream& up_;
  NetworkClient& net_;
  bool chunked_;
  int remaining_ = 0;
  int peeked_ = -1;
  bool eof_ = false;
};

}  // namespace

WeReadClient::Err WeReadClient::post(const char* apiName, JsonDocument& body, JsonDocument& outResp,
                                     int httpTimeoutMs, const JsonDocument* filter) {
  g_lastErrCode = 0;
  g_lastUpgradeMessage.clear();

  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERR("WEREAD", "post(%s): no wifi", apiName);
    return Err::NoWifi;
  }

  const std::string apiKey = WeReadKeyStore::load();
  if (apiKey.empty()) {
    LOG_ERR("WEREAD", "post(%s): no api key", apiName);
    return Err::NoApiKey;
  }

  // Inject required fields. body remains JsonDocument so caller-passed nested
  // arrays/objects survive the injection.
  body["api_name"] = apiName;
  body["skill_version"] = kSkillVersion;

  std::string payload;
  serializeJson(body, payload);

  // TLS client. setInsecure() matches HttpDownloader.cpp:58-59 — same trust
  // posture as OTA / OPDS fetches. Tighten later via esp_crt_bundle_attach if
  // weread.qq.com cert chain matters.
  auto secureClient = std::unique_ptr<NetworkClientSecure>(new (std::nothrow) NetworkClientSecure());
  if (!secureClient) {
    LOG_ERR("WEREAD", "post(%s): OOM TLS client", apiName);
    return Err::Http;
  }
  secureClient->setInsecure();

  HTTPClient http;
  http.setTimeout(httpTimeoutMs);

  if (!http.begin(*secureClient, kGatewayUrl)) {
    LOG_ERR("WEREAD", "post(%s): http.begin failed", apiName);
    return Err::Http;
  }
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("User-Agent", "CrossPoint-ESP32-" CROSSPOINT_VERSION);

  std::string authHeader = "Bearer ";
  authHeader += apiKey;
  http.addHeader("Authorization", authHeader.c_str());

  LOG_DBG("WEREAD", "POST %s (body=%u bytes)", apiName, static_cast<unsigned>(payload.size()));

  // HTTPClient::POST(uint8_t*, size_t) is non-const; cast away to mirror its
  // signature. The buffer is not modified.
  const int httpCode =
      http.POST(reinterpret_cast<uint8_t*>(const_cast<char*>(payload.data())), payload.size());
  if (httpCode != HTTP_CODE_OK) {
    LOG_ERR("WEREAD", "POST %s HTTP %d", apiName, httpCode);
    http.end();
    return Err::Http;
  }

  // Stream the body straight into ArduinoJson via HttpBodyStream — the
  // gateway uses chunked transfer-encoding (no Content-Length), and the
  // wrapper consumes the framing transparently. The optional filter, when
  // supplied, drops fields outside the subclass's whitelist as parsing
  // happens, keeping the JsonDocument small enough for the fragmented heap.
  const int contentLength = http.getSize();
  HttpBodyStream bodyStream(http.getStream(), *secureClient, /*chunked=*/contentLength < 0, contentLength);

  const bool useFilter = filter != nullptr && filter->size() > 0;
  const DeserializationError parseErr =
      useFilter ? deserializeJson(outResp, bodyStream, DeserializationOption::Filter(*filter))
                : deserializeJson(outResp, bodyStream);
  http.end();

  if (parseErr) {
    LOG_ERR("WEREAD", "POST %s parse error: %s (contentLength=%d, filter=%d)", apiName, parseErr.c_str(),
            contentLength, useFilter ? 1 : 0);
    return Err::Parse;
  }

  // Skill upgrade gate — per SKILL.md §通用规则 1, must halt on this field.
  JsonVariantConst upgrade = outResp["upgrade_info"];
  if (!upgrade.isNull()) {
    g_lastUpgradeMessage = upgrade["message"] | "Skill upgrade required";
    LOG_ERR("WEREAD", "POST %s upgrade_info: %s", apiName, g_lastUpgradeMessage.c_str());
    return Err::Upgrade;
  }

  const int errcode = outResp["errcode"] | 0;
  if (errcode != 0) {
    g_lastErrCode = errcode;
    const char* errmsg = outResp["errmsg"] | outResp["errMsg"] | "";
    LOG_ERR("WEREAD", "POST %s errcode=%d errmsg=%s", apiName, errcode, errmsg);
    return Err::Server;
  }

  LOG_DBG("WEREAD", "POST %s OK", apiName);
  return Err::Ok;
}

int WeReadClient::lastErrCode() { return g_lastErrCode; }

const std::string& WeReadClient::lastUpgradeMessage() { return g_lastUpgradeMessage; }

const char* WeReadClient::errorName(Err err) {
  switch (err) {
    case Err::Ok:
      return "Ok";
    case Err::NoWifi:
      return "NoWifi";
    case Err::NoApiKey:
      return "NoApiKey";
    case Err::Http:
      return "Http";
    case Err::Parse:
      return "Parse";
    case Err::Server:
      return "Server";
    case Err::Upgrade:
      return "Upgrade";
  }
  return "Unknown";
}
